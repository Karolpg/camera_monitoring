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
#include <ctime>
#include <sstream>


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

static std::string currentDateTime() {
    time_t now = time(0);
    char buf[128];
    tm  *timeMember = localtime(&now);
    strftime(buf, sizeof(buf), "%Y-%m-%d.%X", timeMember);
    return buf;
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
    m_frameDescr.width = width;
    m_frameDescr.height = height;
    m_frameDescr.components = components;
    m_cameraFps = cameraFps;
    m_frameTime = 1.0 / m_cameraFps;

    uint32_t cnt = 0;
    size_t frameSize = m_frameDescr.width*m_frameDescr.height*m_frameDescr.components;
    for (Frame& frame : m_cyclicBuffer) {
        frame.bufferIdx = cnt++;
        frame.data.resize(frameSize);
    }
}

void FrameControler::addFrame(const uint8_t* data)
{
    assert(data);
    uint64_t prevFrameNr = m_frameCtr;
    uint32_t prevFrameInBuffer = static_cast<uint32_t>(prevFrameNr % m_cyclicBuffer.size());

    uint64_t frameNr = ++m_frameCtr;
    uint32_t frameInBuffer = static_cast<uint32_t>(frameNr % m_cyclicBuffer.size());
    Frame& frame = m_cyclicBuffer[frameInBuffer];
    assert(frameInBuffer == frame.bufferIdx);

    frame.nr = frameNr;
    frame.time = std::chrono::steady_clock::now();
    std::memcpy(frame.data.data(), data, frame.data.size());

    notifyAboutNewFrame();
    feedRecorder(frame);
    runDetection(frame);
}

void FrameControler::runDetection(const Frame &frame)
{
    //if (m_detector && isFrameChanged(m_cyclicBuffer[frameInBuffer], m_cyclicBuffer[prevFrameInBuffer])) {
    if (!m_detector) {
        return;
    }

    if (!m_detectionMutex.try_lock()) {
        return;
    }
    // if detector is not processing, then data is copied into detector cache
    m_detector->setInput(frame.data.data(), m_frameDescr.width, m_frameDescr.height, m_frameDescr.components);
    m_detectionMutex.unlock();

    //
    // we are in one thread and it is safe, but even if not, then detector could be invoked with the same data just few times
    //

    // start worker thread, we suspect (generally know that) that this calculation is CPU expensive and we have to collect/consume incoming frames from camera.
    std::thread( [this, frame = frame.nr, frameInBuffer = frame.bufferIdx]
    {
        const std::lock_guard<std::mutex> lock(m_detectionMutex);

        std::cout << "Detecting for: " << frame << "(" << frameInBuffer << ")\n";
        auto start = std::chrono::steady_clock::now();
        if (m_detector->detect()) {
            std::chrono::duration<double> detectTime = std::chrono::steady_clock::now() - start;
            std::cout << "Detection time: " << detectTime.count() << "[s]\n";

            const auto& results = m_detector->lastResults();
            std::stringstream ss;
            std::set<std::string> labels;
            for (const DetectionResult& dr : results) {
                labels.insert(dr.label);
                ss << "Detected: [" << dr.classId << "] " << dr.label << " " << dr.probablity
                   << " (" << dr.box.x << "x" << dr.box.y << ", " << dr.box.w << "x" << dr.box.h <<  "\n";
            }
            std::string info = ss.str();
            std::cout << info;

            std::string foundObjects;
            for(auto l : labels) {
                foundObjects += l;
                foundObjects += '_';
            }
            std::string filePath = "/tmp/detection_";
            filePath += currentDateTime();
            filePath += '_';
            filePath += foundObjects;
            std::string detectedFrameFilePath = filePath + ".png";
            std::string videoFilePath = filePath + ".mpeg";

            //auto detectedinImg = m_detector->getInImg();
            //PngTools::writePngFile((std::string("/tmp/detectionResult") + std::to_string(frameNr) + "_.png").c_str(),
            //                       detectedinImg.w, detectedinImg.h, detectedinImg.c, detectedinImg.data.data());
            //
            auto detectedOutImg = m_detector->getLabeledInImg();
            PngTools::writePngFile(detectedFrameFilePath.c_str(), detectedOutImg.w, detectedOutImg.h, detectedOutImg.c, detectedOutImg.data.data());

            recording(videoFilePath, frame, frameInBuffer);

            info += "Detection trigger storing video on: " + videoFilePath;
            notifyAboutDetection(info);
        }
        else {
            std::chrono::duration<double> detectTime = std::chrono::steady_clock::now() - start;
            std::cout << "Nothing detected. Detection time: " << detectTime.count() << "[s]\n";
        }
        std::cout.flush();
    }).detach();
}

void FrameControler::recording(const std::string& filename, uint64_t frameNr, uint32_t frameInBuffer)
{
    std::lock_guard<std::mutex> lg(m_recorderMutex);
    if (m_videoRecorder) {
        std::cout << "Continue recording, frame:" << frameNr << "\n";
        m_stopRecordingTime = std::chrono::steady_clock::now() + std::chrono::seconds(10);
    }
    else {
        RecordingDataType rdt;
        std::memset(&rdt, 0, sizeof (rdt));
        rdt.useVideo = true;
        switch (m_frameDescr.components) {
            case 1: rdt.videoFormat = GST_VIDEO_FORMAT_GRAY8; break;
            case 2: rdt.videoFormat = GST_VIDEO_FORMAT_GRAY16_LE; break;
            case 3: rdt.videoFormat = GST_VIDEO_FORMAT_RGB; break;
            case 4: rdt.videoFormat = GST_VIDEO_FORMAT_RGBA; break;
            default: throw std::runtime_error("Invalid components!");
        }
        rdt.videoW = m_frameDescr.width;
        rdt.videoH = m_frameDescr.height;
        rdt.videoFpsN = static_cast<uint32_t>(m_cameraFps);
        rdt.videoFpsD = 1;
        m_videoRecorder = std::unique_ptr<VideoRecorder>(new VideoRecorder(filename, rdt));

        if (m_cyclicBuffer[frameInBuffer].nr == frameNr) {
            uint32_t findFirst = frameInBuffer;
            for (uint32_t i = 1; i < m_cyclicBuffer.size() - 1; ++i) {
                uint32_t prevFrame = (frameInBuffer - i)%m_cyclicBuffer.size();
                if (m_cyclicBuffer[prevFrame].nr != frameNr - i) {
                    findFirst = (prevFrame + 1)%m_cyclicBuffer.size();
                    break;
                }
            }
            std::cout << "Current: " << frameInBuffer << " first: " << findFirst << "\n";
            do
            {
                m_videoRecorder->addFrame(m_cyclicBuffer[findFirst].data);
                ++findFirst;
            }
            while (m_cyclicBuffer[(findFirst-1)%m_cyclicBuffer.size()].nr + 1 == m_cyclicBuffer[findFirst].nr);
        }
        else {
            std::cout << "Unsynchronized! Please set longer cyclic buffer!\n";
            auto detectedinImg = m_detector->getInImg();
            m_videoRecorder->addFrame(detectedinImg.data);
        }
        m_stopRecordingTime = std::chrono::steady_clock::now() + std::chrono::seconds(10);
    }
}

void FrameControler::feedRecorder(const Frame& frame)
{
    std::lock_guard<std::mutex> lg(m_recorderMutex);
    if (std::chrono::steady_clock::now() < m_stopRecordingTime) {
        assert(m_videoRecorder);
        m_videoRecorder->addFrame(frame.data);
    }
    else {
        if (m_videoRecorder) {
            //below save last+1 frame to check last part of video is ok - should not have to many difference especially in time
            //std::string filePath = m_videoRecorder->recordingFilePath();
            //filePath += currentDateTime();
            //filePath += "_endRecording";
            //std::string nextFrameAfterFinishPath = filePath + ".png";
            //PngTools::writePngFile(nextFrameAfterFinishPath.c_str(), m_width, m_height, m_components, frame.data.data());

            std::thread t([this](std::unique_ptr<VideoRecorder> videoRecorder) {
                videoRecorder->waitForFinish();
                std::cout << "Finished recording: " << videoRecorder->recordingFilePath() << "\n";
                notifyAboutVideoReady(videoRecorder->recordingFilePath());
            }, std::move(m_videoRecorder));
            t.detach();
        }
    }
}

void FrameControler::notifyAboutDetection(const std::string &detectionInfo)
{
    const std::lock_guard<std::mutex> lock(m_listenerDetectionMutex);
    for (const auto& tuple : m_detectListener) {
        void* ctx = std::get<0>(tuple);
        OnDetect func = std::get<1>(tuple);
        //TODO reorganize Detector to take Frame instead of Image
        //func(, , detectionInfo, ctx);
    }
}

void FrameControler::notifyAboutVideoReady(const std::string& videoFilePath)
{
    const std::lock_guard<std::mutex> lock(m_listenerVideoReadyMutex);
    for (const auto& tuple : m_videoReadyListener) {
        void* ctx = std::get<0>(tuple);
        OnVideoReady func = std::get<1>(tuple);
        func(videoFilePath, ctx);
    }
}

void FrameControler::notifyAboutNewFrame()
{
    const std::lock_guard<std::mutex> lock(m_listenerCurrentFrameMutex);

    uint32_t frameInBuffer = static_cast<uint32_t>(m_frameCtr % m_cyclicBuffer.size());
    Frame& frame = m_cyclicBuffer[frameInBuffer];

    while (!m_nearestFrameListener.empty()) {
        const auto& tuple = m_nearestFrameListener.front();
        void* ctx = std::get<0>(tuple);
        OnCurrentFrameReady func = std::get<1>(tuple);
        func(frame, m_frameDescr, ctx);
        m_nearestFrameListener.pop();
    }

    for (const auto& tuple : m_currentFrameListener) {
        void* ctx = std::get<0>(tuple);
        OnCurrentFrameReady func = std::get<1>(tuple);
        func(frame, m_frameDescr, ctx);
    }
}

bool FrameControler::isFrameChanged(const Frame& f1, const Frame& f2) const
{
    const uint32_t PIXEL_COUNT_THRESHOLD = static_cast<uint32_t>(m_frameDescr.width*m_frameDescr.height*0.01);
    const uint32_t COMPONENT_THRESHOLD = 15;
    uint32_t componentDiff = 0;

    const uint32_t TIME_POS_X1 = 368;
    const uint32_t TIME_POS_X2 = 452;
    const uint32_t TIME_POS_Y1 = 21;
    const uint32_t TIME_POS_Y2 = 45;

    for (uint32_t c = 0; c < m_frameDescr.components; ++c) {
        for (uint32_t y = 0; y < m_frameDescr.height; ++y) {
            for (uint32_t x = 0; x < m_frameDescr.width; ++x) {
                if(x > TIME_POS_X1 && x < TIME_POS_X2 && y > TIME_POS_Y1 && y < TIME_POS_Y2) {
                    continue; // temporary solution to skip camera time
                }
                uint32_t idx = (y * m_frameDescr.width + x) * m_frameDescr.components + c;
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

    uint32_t pixelDiff = componentDiff/m_frameDescr.components;
    std::cout << "Pixel diff: " << pixelDiff << "\n";

    return (pixelDiff > PIXEL_COUNT_THRESHOLD);
}

bool FrameControler::subscribeOnCurrentFrame(OnCurrentFrameReady notifyFunc, void *ctx, bool notifyOnce)
{
    if (notifyOnce) {
        const std::lock_guard<std::mutex> lock(m_listenerCurrentFrameMutex);
        m_nearestFrameListener.push(std::make_tuple(ctx, notifyFunc));
    }
    else {
        const std::lock_guard<std::mutex> lock(m_listenerCurrentFrameMutex);
        m_currentFrameListener.push_back(std::make_tuple(ctx, notifyFunc));
    }
    return true;
}

bool FrameControler::subscribeOnDetection(OnDetect notifyFunc, void *ctx)
{
    const std::lock_guard<std::mutex> lock(m_listenerDetectionMutex);
    m_detectListener.push_back(std::make_tuple(ctx, notifyFunc));
    return true;
}

bool FrameControler::subscribeOnDetectionVideoReady(OnVideoReady notifyFunc, void *ctx)
{
    const std::lock_guard<std::mutex> lock(m_listenerVideoReadyMutex);
    m_videoReadyListener.push_back(std::make_tuple(ctx, notifyFunc));
    return true;
}

