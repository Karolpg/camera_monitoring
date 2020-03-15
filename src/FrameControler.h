#pragma once

#include <vector>
#include <queue>
#include <cstdint>
#include <chrono>
#include <memory>
#include <mutex>
#include <functional>

#include "Detector.h"
#include "Frame.h"

class VideoRecorder;

class FrameControler
{
public:
    using OnCurrentFrameReady = std::function<void(const Frame& f, const FrameDescr& fd, void* ctx)>;
    using OnDetect = std::function<void(const Frame& f, const FrameDescr& fd, const std::string& detectionInfo, void* ctx)>;
    using OnVideoReady = std::function<void(const std::string& filePath, void* ctx)>;

    FrameControler();
    ~FrameControler();

    void setBufferParams(double duration, double cameraFps, uint32_t width, uint32_t height, uint32_t components);
    void setDetector(const std::shared_ptr<Detector>& detector) { m_detector = detector; }

    void addFrame(const uint8_t* data);

    uint32_t getWidth() const { return m_frameDescr.width; }
    uint32_t getHeight() const { return m_frameDescr.height; }
    uint32_t getComponents() const { return m_frameDescr.components; }

    bool subscribeOnCurrentFrame(OnCurrentFrameReady notifyFunc, void* ctx = nullptr, bool notifyOnce = true);
    bool subscribeOnDetection(OnDetect notifyFunc, void* ctx = nullptr);
    bool subscribeOnDetectionVideoReady(OnVideoReady notifyFunc, void* ctx = nullptr);
private:
    bool isFrameChanged(const Frame& f1, const Frame& f2) const;
    void runDetection(const Frame& frame);
    void recording(const std::string& filename, uint64_t frameNr, uint32_t frameInBuffer);
    void feedRecorder(const Frame& frame);
    void notifyAboutDetection(const std::string& detectionInfo);
    void notifyAboutVideoReady(const std::string& videoFilePath);
    void notifyAboutNewFrame();

    double m_cameraFps = 0.0;
    double m_frameTime = 0.0;

    std::vector<Frame> m_cyclicBuffer;

    uint64_t m_frameCtr = 0;

    FrameDescr m_frameDescr; // common data for every frame

    std::mutex m_detectionMutex;
    std::shared_ptr<Detector> m_detector;

    std::mutex m_recorderMutex;
    std::chrono::steady_clock::time_point m_stopRecordingTime;
    std::unique_ptr<VideoRecorder> m_videoRecorder;

    std::mutex m_listenerCurrentFrameMutex;
    std::mutex m_listenerDetectionMutex;
    std::mutex m_listenerVideoReadyMutex;
    std::queue<std::tuple<void*, OnCurrentFrameReady>> m_nearestFrameListener;
    std::vector<std::tuple<void*, OnCurrentFrameReady>> m_currentFrameListener;
    std::vector<std::tuple<void*, OnDetect>> m_detectListener;
    std::vector<std::tuple<void*, OnVideoReady>> m_videoReadyListener;
};
