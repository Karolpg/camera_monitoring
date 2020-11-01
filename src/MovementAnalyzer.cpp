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
    if (m_descr.components != descr.components) {
        m_firstFrameTime = std::chrono::steady_clock::now();
        m_pyramidFirstFrame = &m_pyramidFrames[0];
        m_pyramidSecondFrame = &m_pyramidFrames[1];
        m_pyramidSecondCpyFrame = &m_pyramidFrames[2];
        m_descr = descr;
        allocateMem();
        buildPyramid(frame, descr, m_pyramidFirstFrame);
        return;
    }

    auto frameTime = std::chrono::steady_clock::now();
    auto timeBetweenFrames = frameTime - m_firstFrameTime;
    double seconds = std::chrono::duration_cast<std::chrono::seconds>(timeBetweenFrames).count();
    if (seconds < TIME_BETWEEN_FRAMES) {
        return;
    }
    m_firstFrameTime = frameTime;
    buildPyramid(frame, descr, m_pyramidSecondFrame);
    *m_pyramidSecondCpyFrame = *m_pyramidSecondFrame; // we need another copy
    analyzeMovement();
    std::swap(m_pyramidFirstFrame, m_pyramidSecondCpyFrame);

    {
        auto processingTime = std::chrono::steady_clock::now() - frameTime;
        double seconds = std::chrono::duration_cast<std::chrono::seconds>(processingTime).count();
        std::cout << "MovementAnalyzer::feedAnalyzer time: " << seconds << "[s]\n";
        std::cout.flush();
    }
}

void MovementAnalyzer::allocateMem()
{
    for (uint32_t f = 0; f < m_pyramidFrames.size(); ++f) {
        m_pyramidFrames[f].clear();
        for (uint32_t size = PYRAMIND_START_SIZE; size >= PYRAMIND_END_SIZE; size = (size >> 1)) {
            FrameF32 img;
            img.data.resize(size*size*m_descr.components);
            m_pyramidFrames[f].push_back(std::move(img));
        }
    }
}

void MovementAnalyzer::buildPyramid(const FrameU8 &frame, const FrameDescr &descr, MovementAnalyzer::Pyramid *pyramid)
{
    ImgUtils::resize(descr.width, descr.height, descr.components, frame.data.data(), ImgUtils::DT_Uint8, ImgUtils::Pixel, 0,
                     PYRAMIND_START_SIZE, PYRAMIND_START_SIZE, m_descr.components, (*pyramid)[0].data.data(), ImgUtils::DT_Float32, ImgUtils::Pixel, 0,
                     false, nullptr, nullptr, nullptr, nullptr, nullptr);


    uint32_t size = PYRAMIND_START_SIZE;
    for (uint32_t ctr = 0; ctr < pyramid->size() - 1; ++ctr) {
        uint32_t nextSize = (size >> 1);
        ImgUtils::resize(size, size, m_descr.components, (*pyramid)[ctr].data.data(), ImgUtils::DT_Float32, ImgUtils::Pixel, 0,
                         nextSize, nextSize, m_descr.components, (*pyramid)[ctr + 1].data.data(), ImgUtils::DT_Float32, ImgUtils::Pixel, 0,
                         false, nullptr, nullptr, nullptr, nullptr, nullptr);
        size = nextSize;
    }
}

void suqareDiff(const FrameF32& src1, const FrameF32& src2, FrameF32 &out)
{
    #pragma omp for
    for (size_t i = 0; i < src1.data.size(); ++i) {
        float diff = src1.data[i] - src2.data[i];
        out.data[i] = diff*diff;
    }
}

inline float squareDiffPatch(const std::vector<float>& basePatch,
                             const FrameF32& otherFrame, const FrameDescr& otherDescr, uint32_t otherX, uint32_t otherY,
                             uint32_t patchSize
                             )
{
    float sum = 0;
    uint32_t patchCtr = 0;
    for (uint32_t y = otherY; y < otherY + patchSize; ++y) {
        for (uint32_t x = otherX; x < otherX + patchSize; ++x) {
            for (uint32_t c = 0; c < otherDescr.components; ++c) {
                uint32_t componentPos = (y*otherDescr.width+x)*otherDescr.components + c;
                float diff = basePatch[patchCtr++] - otherFrame.data[componentPos];
                sum += diff*diff;
            }
        }
    }
    return sum;
}

//inline float squareDiffPatch(const float *patchBase, const float *patchOther, uint32_t patchSize, uint32_t components)
//{
//    uint32_t pixelSize = patchSize * patchSize * components;
//    float sum = 0;
//    for (uint32_t i = 0 ; i < pixelSize; ++i) {
//        float diff = patchBase[i] - patchOther[i];
//        sum += diff * diff;
//    }
//   return sum;
//}

//inline float laplacianPatchDiff(std::vector<float*> &colorpatches, std::vector<float*> &colorfpatches,
//                                 uint32_t x, uint32_t y, uint32_t patchSize, float lambda, uint32_t components)
//{

//    float *patcha, *patchb, *patchfa, *patchfb;

//    //patcha = (float*)colorpatches[x];
//    //patchb = (float*)colorpatches[y];
//    //patchfa = (float*)colorfpatches[x];
//    //patchfb = (float*)colorfpatches[y];

//    float sum = 0;
//    uint32_t pixelSize = patchSize * patchSize * components;
//    float invLambda = 1 - lambda;
//    for (uint32_t i = 0; i < pixelSize; ++i) {
//        sum +=  invLambda*(patcha[i] - patchb[i] ) * (patcha[i]  - patchb[i] );
//        sum +=     lambda*(patchfa[i]- patchfb[i]) * (patchfa[i] - patchfb[i]);
//    }
//    return sum;
//}

static void saveF32(const char* name, const FrameF32& f, const FrameDescr& stepDescr)
{
    FrameU8 b;
    b.data.resize(stepDescr.width * stepDescr.height * stepDescr.components);
    ImgUtils::resize(stepDescr.width, stepDescr.height, stepDescr.components, f.data.data(), ImgUtils::DT_Float32, ImgUtils::Pixel, 0,
                     stepDescr.width, stepDescr.height, stepDescr.components, b.data.data(), ImgUtils::DT_Uint8, ImgUtils::Pixel, 0,
                     false, nullptr, nullptr, nullptr, nullptr, nullptr);

    std::stringstream ss;
    ss << "/tmp/pyramid_" << stepDescr.width << "x" << stepDescr.height << "_" << name;
    std::string fName = ss.str();
    PngTools::writePngFile(fName.c_str(), stepDescr.width, stepDescr.height, stepDescr.components, b.data.data());
}

void MovementAnalyzer::analyzeMovement()
{
    const size_t smallestFrameCtr = m_pyramidSecondFrame->size() - 1;
    FrameDescr stepDescr;
    stepDescr.width = PYRAMIND_END_SIZE;
    stepDescr.height = PYRAMIND_END_SIZE;
    stepDescr.components = m_descr.components;
    for (size_t ctr = smallestFrameCtr; int(ctr) >= 0; --ctr) {
        FrameF32& baseF = (*m_pyramidFirstFrame)[ctr];
        FrameF32& nextF = (*m_pyramidSecondFrame)[ctr];

        // remove background
        FrameF32 diff;
        diff.data.resize(baseF.data.size(), 0);
        suqareDiff(baseF, nextF, diff);

        if (stepDescr.width == 256) {
            //saveF32("base", baseF, stepDescr);
            //saveF32("next", nextF, stepDescr);
            saveF32("diff", diff, stepDescr);
        }

        const float zeroThreshold = 0.000015234f;
        #pragma omp for
        for (size_t i = 0; i < diff.data.size(); ++i)
        {
            if (diff.data[i] <= zeroThreshold)
            {
                baseF.data[i] = 0;
                nextF.data[i] = 0; // we can't do this - because of swapping at the end of calculation
                                     // instead of this we change it in linearch patch
            }
        }
/*
        // find patch
        constexpr uint32_t maskSize = 3;
        std::vector<Pos> movement(stepDescr.width * stepDescr.height);

        // classify diff of frames tiled by patch
        enum PixelProps{IsBg, HasObj, Invalid};
        FrameU8 bgCheck;                       // Each pixel represents patch starting point and express whole patch.
        bgCheck.data.resize(stepDescr.width * stepDescr.height, 0); // Here is assumption about only one component!
        #pragma omp for
        for (uint32_t y = 0; y < stepDescr.height; ++y) {
            for (uint32_t x = 0; x < stepDescr.width; ++x) {
                if (x + maskSize >= stepDescr.width || y + maskSize >= stepDescr.height) {
                    bgCheck.data[y*stepDescr.width + x] = static_cast<uint8_t>(Invalid);
                    continue;
                }
                float sumToCheck = 0;
                for (uint32_t mx = 0; mx < maskSize; ++mx) {
                    for (uint32_t my = 0; my < maskSize; ++my) {
                        for (uint32_t c = 0; c < stepDescr.components; ++c)
                        sumToCheck += baseF.data[((y + my) * stepDescr.width + x + mx) * stepDescr.components + c];
                    }
                }
                if (sumToCheck > zeroThreshold) {
                    bgCheck.data[y * stepDescr.width + x] = static_cast<uint8_t>(HasObj);
                }
            }
        }

        // main loop
        uint32_t maskInW = stepDescr.width / maskSize;
        uint32_t maskInH = maskInW;
        std::vector<float> currentBasePatch(maskSize * maskSize * stepDescr.components);
        #pragma omp for
        for (uint32_t maskYPos = 0; maskYPos < maskInH; ++maskYPos) {
            for (uint32_t maskXPos = 0; maskXPos < maskInW; ++maskXPos) {

                uint32_t basePixPosY = maskYPos*maskSize;
                uint32_t basePixPosX = maskXPos*maskSize;

                // check is it background
                if (bgCheck.data[basePixPosY * stepDescr.width + basePixPosX] == static_cast<uint8_t>(IsBg)) {
                    continue;
                }
                // optymize use of current base patch by making it as linear memory
                for (uint32_t y = 0; y < maskSize; ++y) {
                    uint32_t baseY = basePixPosY + y;
                    std::copy(&baseF.data[(baseY * stepDescr.width + basePixPosX) * stepDescr.components]
                            , &baseF.data[(baseY * stepDescr.width + basePixPosX + maskSize) * stepDescr.components]
                            , currentBasePatch.data() + y * maskSize * stepDescr.components);
                }

                //we should provide how many meters are in one pixel - now assume 10% of size but no longer than 7 masks
                const uint32_t percentageOfSize = std::max(static_cast<uint32_t>(0.1 * stepDescr.width), maskSize);
                constexpr uint32_t maxMaskCheck = 7;
                const uint32_t pixelToCheck = std::min(percentageOfSize, maxMaskCheck*maskSize);

                int32_t startCheckingY = static_cast<int32_t>(basePixPosY) - static_cast<int32_t>(pixelToCheck);
                uint32_t stopCheckingY = basePixPosY + maskSize + pixelToCheck;
                uint32_t startY = static_cast<uint32_t>(std::max(0, startCheckingY));
                uint32_t stopY = std::min(stepDescr.height - maskSize, stopCheckingY);

                int32_t startCheckingX = static_cast<int32_t>(basePixPosX) - static_cast<int32_t>(pixelToCheck);
                uint32_t stopCheckingX = basePixPosX + maskSize + pixelToCheck;
                uint32_t startX = static_cast<uint32_t>(std::max(0, startCheckingX));
                uint32_t stopX = std::min(stepDescr.width - maskSize, stopCheckingX);

                float similarityValue = 99999999.0f;
                for (uint32_t y = startY; y < stopY; ++y) {
                    for (uint32_t x = startX; x < stopX; ++x) {

                        float currentSimilarity = squareDiffPatch(currentBasePatch, nextF, stepDescr, x, y, maskSize);
                        // take minimum distance
                        if (similarityValue > currentSimilarity) {
                            similarityValue = currentSimilarity;
                            auto& patchMove = movement[basePixPosY * stepDescr.width + basePixPosX];
                            patchMove.x = static_cast<uint16_t>(x);
                            patchMove.y = static_cast<uint16_t>(y);
                        }

                    }
                }

            }
        }

*/
        // prepare to next frame
        stepDescr.width <<= 1;
        stepDescr.height = stepDescr.width;
    }
}



