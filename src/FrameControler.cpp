#include "FrameControler.h"
#include "PngTools.h"
#include "VideoRecorder.h"

#include <algorithm>
#include <assert.h>
#include <chrono>
#include <cstring>
#include <string>
#include <iostream>
#include <thread>
#include <mutex>
#include <stdexcept>


static bool storeFrame(const Frame& frame, uint32_t width, uint32_t height, uint32_t components)
{
    static const std::string path("/tmp/");
    std::string fileName = std::to_string(frame.nr);
    std::string pathFileName = path + fileName + ".png";
    if (!PngTools::writePngFile(pathFileName.c_str(), width, height, components, frame.data.data())) {
        std::cerr << "Can't write png file!!!\n";
        return false;
    }
    return true;
}

FrameControler::FrameControler()
{

}

FrameControler::~FrameControler()
{

}


void FrameControler::setBufferParams(double duration, double cameraFps, uint32_t width, uint32_t height, uint32_t components)
{
    assert(width);
    assert(height);
    assert(components);
    assert(duration > 0.0001);
    assert(cameraFps > 0.1);
    uint32_t frames = static_cast<uint32_t>(duration*cameraFps);
    frames = std::max(static_cast<uint32_t>(1), frames);
    m_cyclicBuffer.resize(frames);
    m_width = width;
    m_height = height;
    m_components = components;
    m_cameraFps = cameraFps;
    m_frameTime = 1.0 / m_cameraFps;

    for (Frame& frame : m_cyclicBuffer) {
        frame.data.resize(m_width*m_height*m_components);
    }
}

void FrameControler::addFrame(const uint8_t* data)
{
    assert(data);
    uint64_t prevFrameNr = m_frameCtr;
    uint32_t prevFrameInBuffer = static_cast<uint32_t>(prevFrameNr % m_cyclicBuffer.size());

    uint64_t frameNr = ++m_frameCtr;
    uint32_t frameInBuffer = static_cast<uint32_t>(frameNr % m_cyclicBuffer.size());
    m_cyclicBuffer[frameInBuffer].nr = frameNr;
    m_cyclicBuffer[frameInBuffer].time = std::chrono::steady_clock::now();
    std::memcpy(m_cyclicBuffer[frameInBuffer].data.data(), data, m_width*m_height*m_components);

    {
        std::lock_guard<std::mutex> lg(m_recorderMutex);
        if (std::chrono::steady_clock::now() < m_stopRecordingTime) {
            assert(m_videoRecorder);
            m_videoRecorder->addFrame(m_cyclicBuffer[frameInBuffer].data);
        }
        else {
            if (m_videoRecorder) {
                m_videoRecorder->finishRecording();
                m_videoRecorder.reset();
            }
        }
    }

/*
    {
        if(!m_videoRecorder) {
            std::string videoFilePath = "/tmp/detection_";
            static int ctr = 0;
            videoFilePath += std::to_string(ctr++);
            videoFilePath += ".mpeg";
            RecordingDataType rdt;
            std::memset(&rdt, 0, sizeof (rdt));
            rdt.useVideo = true;
            switch (m_components) {
                case 1: rdt.videoFormat = GST_VIDEO_FORMAT_GRAY8; break;
                case 2: rdt.videoFormat = GST_VIDEO_FORMAT_GRAY16_LE; break;
                case 3: rdt.videoFormat = GST_VIDEO_FORMAT_RGB; break;
                case 4: rdt.videoFormat = GST_VIDEO_FORMAT_RGBA; break;
                default: throw std::runtime_error("Invalid components!");
            }
            rdt.videoW = m_width;
            rdt.videoH = m_height;
            rdt.videoFpsN = static_cast<uint32_t>(m_cameraFps);
            rdt.videoFpsD = 1;
            m_videoRecorder = std::unique_ptr<VideoRecorder>(new VideoRecorder(videoFilePath, rdt));
            m_stopRecordingTime = std::chrono::steady_clock::now() + std::chrono::seconds(10);
        }
        else {

            if (std::chrono::steady_clock::now() < m_stopRecordingTime) {
                m_videoRecorder->addFrame(m_cyclicBuffer[frameInBuffer].data);
            }
            else {
                m_videoRecorder->finishRecording();
                m_videoRecorder.reset();
            }
        }
    }
    */

    //std::chrono::duration<double> distanceBetweenFrames = m_cyclicBuffer[frameInBuffer].time - m_cyclicBuffer[prevFrameInBuffer].time;
    //if (std::abs(distanceBetweenFrames.count() - m_frameTime) > m_frameTime*0.01) {
    //    std::cout << "Warning! Frame provided too late: " << distanceBetweenFrames.count() - m_frameTime << "[s] fault. Distance: " << distanceBetweenFrames.count() << "[s] \n";
    //}

    //if (m_detector && isFrameChanged(m_cyclicBuffer[frameInBuffer], m_cyclicBuffer[prevFrameInBuffer])) {
    if (m_detector) {
        static std::mutex mtx;

        if (mtx.try_lock()) {
            // if detector is not processing, then data is copied into detector cache
            m_detector->setInput(m_cyclicBuffer[frameInBuffer].data.data(), m_width, m_height, m_components);
            mtx.unlock();

            //
            // we are in one thread and it is safe, but even if not, then detector could be invoked with the same data just few times
            //

            // start worker thread, we suspect (generally know that) that this calculation is CPU expensive and we have to collect/consume incoming frames from camera.
            std::thread( [this, frameNr, frameInBuffer]
            {
                const std::lock_guard<std::mutex> lock(mtx);

                std::cout << "Detecting for: " << frameNr << "(" << frameInBuffer << ")\n";
                auto start = std::chrono::steady_clock::now();
                if (m_detector->detect()) {
                    std::chrono::duration<double> detectTime = std::chrono::steady_clock::now() - start;
                    std::cout << "Detection time: " << detectTime.count() << "[s]\n";

                    auto results = m_detector->lastResults();
                    for (const DetectionResult& dr : results) {
                            std::cout << "Detected: [" << dr.classId << "] " << dr.label << " " << dr.probablity
                                      << " (" << dr.box.x << "x" << dr.box.y << ", " << dr.box.w << "x" << dr.box.h <<  "\n";
                    }

                    auto detectedinImg = m_detector->getInImg();
                    //PngTools::writePngFile((std::string("/tmp/detectionResult") + std::to_string(frameNr) + "_.png").c_str(),
                    //                       detectedinImg.w, detectedinImg.h, detectedinImg.c, detectedinImg.data.data());
                    //
                    //auto detectedOutImg = m_detector->getLabeledInImg();
                    //PngTools::writePngFile((std::string("/tmp/detectionResult") + std::to_string(frameNr) + ".png").c_str(),
                    //                       detectedOutImg.w, detectedOutImg.h, detectedOutImg.c, detectedOutImg.data.data());
                    //PngTools::writePngFile("/tmp/detectionResult.png",
                    //                       detectedOutImg.w, detectedOutImg.h, detectedOutImg.c, detectedOutImg.data.data());

                    {
                        std::lock_guard<std::mutex> lg(m_recorderMutex);
                        if (m_videoRecorder) {
                            m_stopRecordingTime = std::chrono::steady_clock::now() + std::chrono::seconds(10);
                        }
                        else {
                            std::string videoFilePath = "/tmp/detection_";
                            videoFilePath += ".mpeg";
                            RecordingDataType rdt;
                            std::memset(&rdt, 0, sizeof (rdt));
                            rdt.useVideo = true;
                            switch (m_components) {
                                case 1: rdt.videoFormat = GST_VIDEO_FORMAT_GRAY8; break;
                                case 2: rdt.videoFormat = GST_VIDEO_FORMAT_GRAY16_LE; break;
                                case 3: rdt.videoFormat = GST_VIDEO_FORMAT_RGB; break;
                                case 4: rdt.videoFormat = GST_VIDEO_FORMAT_RGBA; break;
                                default: throw std::runtime_error("Invalid components!");
                            }
                            rdt.videoW = m_width;
                            rdt.videoH = m_height;
                            rdt.videoFpsN = static_cast<uint32_t>(m_cameraFps);
                            rdt.videoFpsD = 1;
                            m_videoRecorder = std::unique_ptr<VideoRecorder>(new VideoRecorder(videoFilePath, rdt));

                            if (m_cyclicBuffer[frameInBuffer].nr == frameNr) {
                                uint32_t findFirst = frameInBuffer;
                                for (uint32_t i = 1; i < m_cyclicBuffer.size() - 1; ++i) {
                                    uint32_t prevFrame = (frameInBuffer - i)%m_cyclicBuffer.size();
                                    if (m_cyclicBuffer[prevFrame].nr != frameNr - i) {
                                        findFirst = (prevFrame + 1)%m_cyclicBuffer.size();
                                        break;
                                    }
                                }
                                do
                                {
                                    m_videoRecorder->addFrame(m_cyclicBuffer[findFirst].data);
                                    ++findFirst;
                                }
                                while (m_cyclicBuffer[(findFirst-1)%m_cyclicBuffer.size()].nr + 1 == m_cyclicBuffer[findFirst].nr);
                            }
                            else {
                                std::cout << "Unsynchronized! Please set longer cyclic buffer!\n";
                                m_videoRecorder->addFrame(detectedinImg.data);
                            }
                            m_stopRecordingTime = std::chrono::steady_clock::now() + std::chrono::seconds(10);
                        }
                    }
                }
                else {
                    std::chrono::duration<double> detectTime = std::chrono::steady_clock::now() - start;
                    std::cout << "Nothing detected. Detection time: " << detectTime.count() << "[s]\n";
                }
                std::cout.flush();
            }).detach();
        }
//        else {
//            std::cout << "Busy\n";
//            std::cout.flush();
//        }
    }
}

bool FrameControler::isFrameChanged(const Frame& f1, const Frame& f2)
{
    const uint32_t PIXEL_COUNT_THRESHOLD = static_cast<uint32_t>(m_width*m_height*0.01);
    const uint32_t COMPONENT_THRESHOLD = 15;
    uint32_t componentDiff = 0;

    const uint32_t TIME_POS_X1 = 368;
    const uint32_t TIME_POS_X2 = 452;
    const uint32_t TIME_POS_Y1 = 21;
    const uint32_t TIME_POS_Y2 = 45;

    for (uint32_t c = 0; c < m_components; ++c) {
        for (uint32_t y = 0; y < m_height; ++y) {
            for (uint32_t x = 0; x < m_width; ++x) {
                if(x > TIME_POS_X1 && x < TIME_POS_X2 && y > TIME_POS_Y1 && y < TIME_POS_Y2) {
                    continue; // temporary solution to skip camera time
                }
                uint32_t idx = (y * m_width + x) * m_components + c;
                uint32_t localDiff = f1.data[idx] > f2.data[idx] ? f1.data[idx] - f2.data[idx] : f2.data[idx] - f1.data[idx];
                if (localDiff > COMPONENT_THRESHOLD) {
                    ++componentDiff;
                }
            }
        }
    }

    //std::vector<uint8_t> timePixels(m_components*(TIME_POS_X2-TIME_POS_X1)*(TIME_POS_Y2-TIME_POS_Y1));
    //for (uint32_t y = TIME_POS_Y1; y <TIME_POS_Y2; ++y) {
    //        uint32_t srcIdx = (y*m_width + TIME_POS_X1)*m_components;
    //        uint32_t dstIdx = (y-TIME_POS_Y1)*(TIME_POS_X2-TIME_POS_X1)*m_components;
    //        std::memcpy(&timePixels[dstIdx], &f1.data[srcIdx], (TIME_POS_X2-TIME_POS_X1)*m_components);
    //}
    //writePngFile("/tmp/timeTest.png", TIME_POS_X2-TIME_POS_X1, TIME_POS_Y2-TIME_POS_Y1, timePixels.data());

    uint32_t pixelDiff = componentDiff/m_components;
    std::cout << "Pixel diff: " << pixelDiff << "\n";

    return (pixelDiff > PIXEL_COUNT_THRESHOLD);
}


