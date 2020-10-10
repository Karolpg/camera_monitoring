#include "MovementAnalyzer.h"

#include "ImgUtils.h"

MovementAnalyzer::MovementAnalyzer()
{
}

void MovementAnalyzer::feedAnalyzer(const FrameU8 &frame, const FrameDescr &descr)
{
    if (m_descr.components != descr.components) {
        m_firstFrameTime = std::chrono::steady_clock::now();
        m_pyramidFirstFrame = &m_pyramidFrames[0];
        m_pyramidSecondFrame = &m_pyramidFrames[1];
        m_descr = descr;
        allocateMem();
        buildPyramid(frame, descr, m_pyramidFirstFrame);
        return;
    }

    auto frameTime = std::chrono::steady_clock::now();
    auto timeBetweenFrames = frameTime - m_firstFrameTime;
    if (timeBetweenFrames.count() < TIME_BETWEEN_FRAMES) {
        return;
    }
    m_firstFrameTime = frameTime;
    buildPyramid(frame, descr, m_pyramidSecondFrame);
    analyzeMovement();
    std::swap(m_pyramidFirstFrame, m_pyramidSecondFrame);
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
    for (size_t i = 0; i < src1.data.size(); ++i)
    {
        float diff = src1.data[i] - src2.data[i];
        out.data[i] = diff*diff;
    }
}

void MovementAnalyzer::analyzeMovement()
{
    const size_t smallestFrameCtr = m_pyramidSecondFrame->size() - 1;
    uint32_t frameW = PYRAMIND_END_SIZE;
    uint32_t frameH = frameW;
    for (size_t ctr = smallestFrameCtr; ctr <= 0; --ctr) {
        FrameF32& baseF = (*m_pyramidFirstFrame)[ctr];
        FrameF32& nextF = (*m_pyramidSecondFrame)[ctr];


        // remove background
        FrameF32 diff;
        diff.data.resize(baseF.data.size(), 0);
        suqareDiff(baseF, nextF, diff);

        const float zeroThreshold = 0.000015234f;
        #pragma omp for
        for (size_t i = 0; i < diff.data.size(); ++i)
        {
            if (diff.data[i] <= zeroThreshold)
            {
                baseF.data[i] = 0;
                nextF.data[i] = 0;
            }
        }

        // find patch
        constexpr uint32_t maskSize = 3;
        FrameU8 movement;
        movement.data.resize(frameW*frameH*2, 0);

        enum PixelProps{IsBg, HasObj, Invalid};
        FrameU8 bgCheck;
        bgCheck.data.resize(frameW*frameH, 0);
        for (uint32_t y = 0; y < frameH; ++y) {
            for (uint32_t x = 0; x < frameW; ++x) {
                if (x + maskSize >= frameW || y + maskSize >= frameH) {
                    bgCheck.data[y*frameW + x] = static_cast<uint8_t>(Invalid);
                    continue;
                }
                float sumToCheck = 0;
                for (uint32_t mx = 0; mx < maskSize; ++mx) {
                    for (uint32_t my = 0; my < maskSize; ++my) {
                        for (uint32_t c = 0; c < m_descr.components; ++c)
                        sumToCheck += baseF.data[((y + my)*frameW + x + mx)*m_descr.components + c];
                    }
                }
                if (sumToCheck > zeroThreshold) {
                    bgCheck.data[y*frameW + x] = static_cast<uint8_t>(HasObj);
                }
            }
        }


        uint32_t maskInW = frameW / maskSize;
        uint32_t maskInH = maskInW;
        for(uint32_t maskYPos = 0; maskYPos < maskInH; ++maskYPos) {
            for(uint32_t maskXPos = 0; maskXPos < maskInW; ++maskXPos) {

                uint32_t basePixPosY = maskYPos*maskSize;
                uint32_t basePixPosX = maskXPos*maskSize;

                // check is it background
                if (bgCheck.data[basePixPosY*frameW + basePixPosX] == static_cast<uint8_t>(IsBg)) {
                    continue;
                }

                //we should provide how many meters are in one pixel - now assume 10% of size but no longer than 7 masks
                const uint32_t percentageOfSize = std::max(static_cast<uint32_t>(0.1*frameW), maskSize);
                constexpr uint32_t maxMaskCheck = 7;
                const uint32_t pixelToCheck = std::min(percentageOfSize, maxMaskCheck*maskSize);

                int32_t startCheckingY = static_cast<int32_t>(basePixPosY) - static_cast<int32_t>(pixelToCheck);
                uint32_t stopCheckingY = basePixPosY + maskSize + pixelToCheck;
                uint32_t startY = static_cast<uint32_t>(std::max(0, startCheckingY));
                uint32_t stopY = std::min(frameH - maskSize, stopCheckingY);

                int32_t startCheckingX = static_cast<int32_t>(basePixPosX) - static_cast<int32_t>(pixelToCheck);
                uint32_t stopCheckingX = basePixPosX + maskSize + pixelToCheck;
                uint32_t startX = static_cast<uint32_t>(std::max(0, startCheckingX));
                uint32_t stopX = std::min(frameW - maskSize, stopCheckingX);





            }
        }
        //baseF


        // prepare to next frame
        frameW <<= 1;
        frameH = frameW;
    }
}


