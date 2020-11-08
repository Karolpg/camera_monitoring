#include "MovementAnalyzer.h"

#include "ImgUtils.h"

#include <string>
#include <sstream>
#include <iostream>
#include "PngTools.h"

namespace  {

struct Pos
{
    uint16_t x = 0xffff;
    uint16_t y = 0xffff;
};

}


MovementAnalyzer::MovementAnalyzer()
{
}

void MovementAnalyzer::feedAnalyzer(const FrameU8 &frame, const FrameDescr &descr)
{
    if (m_descrOrg.components != descr.components
        || m_descrOrg.width != descr.width
        || m_descrOrg.height != descr.height) {
        m_firstFrameTime = std::chrono::steady_clock::now();
        m_descrOrg = descr;
        m_descrBase.width = PREFERED_SIZE;
        m_descrBase.height = PREFERED_SIZE;
        m_descrBase.components = descr.components;
        allocateMem();
        m_baseFrame = &m_cacheBase[0];
        m_nextFrame = &m_cacheBase[1];
        scaleFrame(frame, descr, m_baseFrame);
        return;
    }

    auto frameTime = std::chrono::steady_clock::now();
    auto timeBetweenFrames = frameTime - m_firstFrameTime;
    double seconds = std::chrono::duration_cast<std::chrono::seconds>(timeBetweenFrames).count();
    if (seconds < TIME_BETWEEN_FRAMES) {
        return;
    }
    m_firstFrameTime = frameTime;
    scaleFrame(frame, descr, m_nextFrame);
    analyzeMovement();
    std::swap(m_baseFrame, m_nextFrame);

    {
        auto processingTime = std::chrono::steady_clock::now() - frameTime;
        double seconds = std::chrono::duration_cast<std::chrono::seconds>(processingTime).count();
        double micro = std::chrono::duration_cast<std::chrono::microseconds>(processingTime).count();
        std::cout << "MovementAnalyzer::feedAnalyzer time: " << seconds << "[s]" << micro << "[us]\n";
        std::cout.flush();
    }
}

void MovementAnalyzer::allocateMem()
{
    for (uint32_t f = 0; f < m_cache.size(); ++f) {
        m_cache[f].data.resize(m_descrBase.width*m_descrBase.height);
    }

    for (uint32_t f = 0; f < m_cacheBase.size(); ++f) {
        m_cacheBase[f].data.resize(m_descrBase.width*m_descrBase.height*m_descrBase.components);
    }
}

void MovementAnalyzer::scaleFrame(const FrameU8 &frame, const FrameDescr &descr, FrameU8 *outFrame) {
    ImgUtils::resize(descr.width, descr.height,  descr.components, frame.data.data(), ImgUtils::DT_Uint8, ImgUtils::Pixel, 0,
                     m_descrBase.width, m_descrBase.height, m_descrBase.components, outFrame->data.data(), ImgUtils::DT_Uint8, ImgUtils::Pixel, 0,
                     false, nullptr, nullptr, nullptr, nullptr, nullptr);
}

void squareDiff(const FrameU8& src1, const FrameU8& src2, const FrameDescr &srcDescr, FrameU16 &out)
{
    #pragma omp for
    for (uint32_t i = 0; i < static_cast<uint32_t>(src1.data.size()); i += srcDescr.components) {
        uint32_t outPos = i / srcDescr.components;
        out.data[outPos] = 0;
        for (uint32_t c = 0; c < srcDescr.components; ++c) {
            float diff = static_cast<float>(src1.data[i + c]) - src2.data[i + c];
            out.data[outPos] += static_cast<uint16_t>(diff*diff);
        }
    }
}

static void saveU16(const char* name, const FrameU16& f)
{
    FrameU8 b;
    b.data.resize(f.data.size(), 0);
    for (uint32_t i = 0; i < f.data.size(); ++i) {
        //b.data[ctr++] = uint8_t(255 * float(u16)/float(255*255*3));
        if (f.data[i]) {
            b.data[i] = uint8_t(150);
        }
    }

    std::stringstream ss;
    uint32_t w = 512;
    uint32_t h = 512;
    ss << "/tmp/MovementAnalyzer_" << w << "x" << h << "_" << name;
    std::string fName = ss.str();
    PngTools::writePngFile(fName.c_str(), w, h, 1, b.data.data());
}

static void saveU8(const char* name, const FrameU8& f, const FrameDescr& descr)
{
    std::stringstream ss;
    ss << "/tmp/MovementAnalyzer_" << descr.width << "x" << descr.height << "_" << name;
    std::string fName = ss.str();
    PngTools::writePngFile(fName.c_str(), descr.width, descr.height, descr.components, f.data.data());
}

void MovementAnalyzer::analyzeMovement()
{
    squareDiff(*m_baseFrame, *m_nextFrame, m_descrBase, m_cache[0]);
    //saveU8("base", *m_baseFrame, m_descrBase);
    //saveU8("next", *m_nextFrame, m_descrBase);

    const uint16_t zeroThreshold = uint16_t(10*10 * m_descrBase.components); // each component square diff is lower than 5^2
    #pragma omp for
    for (size_t i = 0; i < m_cache[0].data.size(); ++i)
    {
        if (m_cache[0].data[i] <= zeroThreshold)
        {
            m_cache[0].data[i] = 0;
        }
    }

    saveU16("smplified", m_cache[0]);

    makeRegions();

    m_movementDetected = false;
    for (const auto &countPair : m_regions) {
        if (countPair.second >= REGION_THRESHOLD) {
            m_movementDetected = true;
            break;
        }
    }


    if (m_movementDetected) {
        static int ctr = 0;
        std::cout << "Movement detected: " << ++ctr << "\n";
        std::cout.flush();
    }
}

void MovementAnalyzer::makeRegions()
{
    m_regions.clear();
    std::map<uint16_t, uint32_t> localRegions; // key = regionId, value = pixel count
    std::map<uint16_t, uint16_t> regionsConnections; // key = regionId, value = master region
    uint16_t newRegionId = 1;

    auto& squareDiffImg = m_cache[0].data;
    // first element
    if (squareDiffImg[0] > 0) {
        squareDiffImg[0] = newRegionId;
        localRegions[newRegionId] = 1;
        ++newRegionId;
    }
    // first line
    for (uint32_t x = 1; x < m_descrBase.width; ++x) {
        if (squareDiffImg[x] > 0) {
            if (squareDiffImg[x-1] > 0) {
                uint16_t prevRegion = squareDiffImg[x-1];
                squareDiffImg[x] = prevRegion;
                localRegions[prevRegion] += 1;
            }
            else {
                squareDiffImg[x] = newRegionId;
                localRegions[newRegionId] = 1;
                ++newRegionId;
            }
        }
    }

    // rest elements
    for (uint32_t y = 1; y < m_descrBase.height; ++y) {
        uint32_t pixelRow = y * m_descrBase.width;
        uint32_t pixelUpRow = pixelRow - m_descrBase.width;

        if (squareDiffImg[pixelRow] > 0) {
            if (squareDiffImg[pixelUpRow] > 0) {
                uint16_t prevRegion = squareDiffImg[pixelUpRow];
                squareDiffImg[pixelRow] = prevRegion;
                localRegions[prevRegion] += 1;
            }
            else {
                squareDiffImg[pixelRow] = newRegionId;
                localRegions[newRegionId] = 1;
                ++newRegionId;
            }
        }

        for (uint32_t x = 1; x < m_descrBase.width; ++x) {
            uint32_t pixelPos = pixelRow + x;
            if (squareDiffImg[pixelPos] > 0) {
                uint16_t prevRegion = squareDiffImg[pixelPos-1];
                uint16_t upRegion = squareDiffImg[pixelPos-m_descrBase.width];
                if (prevRegion > 0 || upRegion > 0) {
                    uint16_t useRegion = upRegion == 0 ? prevRegion : upRegion;

                    if (prevRegion > 0 && upRegion > 0 && prevRegion != upRegion) {
                        regionsConnections[prevRegion] = upRegion;
                    }

                    squareDiffImg[x] = useRegion;
                    localRegions[useRegion] += 1;
                }
                else {
                    squareDiffImg[pixelPos] = newRegionId;
                    localRegions[newRegionId] = 1;
                    ++newRegionId;
                }
            }
        }
    }

    // sum connected regions
    for (auto& connection : regionsConnections) {
        uint32_t &childCount = localRegions[connection.first];
        localRegions[connection.second] += childCount;
        childCount = 0;
    } 
    // take only positive
    for (const auto &countPair : localRegions) {
        if (countPair.second > 0) {
            m_regions.insert(countPair);
        }
    }

    // debug
    {
        if(m_regions.empty())
        {
            return;
        }

        for (auto& pix : squareDiffImg) {
            if (pix && regionsConnections.count(pix)) {
                auto it = regionsConnections.find(pix);
                uint16_t parent = 0;
                while (it != regionsConnections.end()) {
                    parent = it->second;
                    it = regionsConnections.find(parent);
                }
                pix = parent;
            }
        }

        std::map<uint32_t, uint16_t> sortedSize;
        for (const auto &countPair : m_regions) {
            sortedSize.insert(std::make_pair(countPair.second, countPair.first));
        }
        std::map<uint16_t, uint8_t> regionToColor;
        uint8_t currentColor = 255;
        for (auto it = m_regions.end(); it != m_regions.begin() && currentColor >= 20;) {
            --it;
            regionToColor[it->first] = currentColor;
            currentColor -= 20;
        }

        FrameU8 b;
        b.data.resize(squareDiffImg.size(), 0);
        for (uint32_t i = 0; i < b.data.size(); ++i) {
            if (squareDiffImg[i]) {
                auto colorIt = regionToColor.find(squareDiffImg[i]);
                b.data[i] = colorIt != regionToColor.end() ? colorIt->second : 0;
            }
        }

        std::stringstream ss;
        uint32_t w = 512;
        uint32_t h = 512;
        ss << "/tmp/MovementAnalyzer_" << w << "x" << h << "_" << "regions";
        std::string fName = ss.str();
        PngTools::writePngFile(fName.c_str(), w, h, 1, b.data.data());
    }
}


