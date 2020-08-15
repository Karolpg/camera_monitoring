#pragma once

#include <vector>
#include <queue>
#include <cstdint>
#include <chrono>
#include <memory>
#include <mutex>
#include <functional>
#include <thread>
#include <condition_variable>
#include <list>

#include "Detector.h"
#include "Frame.h"
#include "Config.h"

class VideoRecorder;

class FrameController
{
public:
    using OnDie = std::function<void(void *ctx)>;
    using OnCurrentFrameReady = std::function<void(const FrameU8& f, const FrameDescr& fd, void* ctx)>;
    using OnDetect = std::function<void(const FrameU8& f, const FrameDescr& fd, const std::string& detectionInfo, void* ctx)>;
    using OnVideoReady = std::function<void(const std::string& filePath, void* ctx)>;

    FrameController(const Config& cfg);
    ~FrameController();

    void setBufferParams(double duration, double cameraFps, uint32_t width, uint32_t height, uint32_t components);
    void setDetector(const std::shared_ptr<Detector>& detector);

    void addFrame(const uint8_t* data);

    uint32_t getWidth() const { return m_frameDescr.width; }
    uint32_t getHeight() const { return m_frameDescr.height; }
    uint32_t getComponents() const { return m_frameDescr.components; }

    void subscribeOnCurrentFrame(OnCurrentFrameReady notifyFunc, void* ctx = nullptr, bool notifyOnce = true);
    void unsubscribeOnCurrentFrame(OnCurrentFrameReady notifyFunc, void* ctx = nullptr); // only for notifyOnce = false

    void subscribeOnDetection(OnDetect notifyFunc, void* ctx = nullptr);
    void unsubscribeOnDetection(OnDetect notifyFunc, void* ctx = nullptr);

    void subscribeOnDetectionVideoReady(OnVideoReady notifyFunc, void* ctx = nullptr);
    void unsubscribeOnDetectionVideoReady(OnVideoReady notifyFunc, void* ctx = nullptr);

    void subscribeOnDie(OnDie notifyFunc, void* ctx);
    void unsubscribeOnDie(OnDie notifyFunc, void* ctx);
private:
    enum RecordingResult {
        ContinuePrevVideo,
        StartedNewVideo,
    };

    struct DetectionData {
        std::mutex detectionMutex;
        uint64_t frame;
        uint32_t frameInBuffer;
        bool jobIsReady = false;
        std::shared_ptr<Detector> detector;
    };

    bool isFrameChanged(const FrameU8& f1, const FrameU8& f2) const;
    void runDetection(const FrameU8& frame);
    void detect();
    void detectionThreadFunc();
    RecordingResult recording(const std::string& filename, uint64_t frameNr, uint32_t frameInBuffer);
    void feedRecorder(const FrameU8& frame);
    void notifyAboutDetection(const std::string& detectionInfo, const FrameU8 &f, const FrameDescr &fd);
    void notifyAboutVideoReady(const std::string& videoFilePath);
    void notifyAboutNewFrame();

    double m_cameraFps = 0.0;
    double m_frameTime = 0.0;

    std::vector<FrameU8> m_cyclicBuffer;

    uint64_t m_frameCtr = 0;
    FrameDescr m_frameDescr; // common data for every frame

    DetectionData m_detectionData;
    volatile bool m_detectorThreadIsRunning = true;
    std::mutex m_waitForDetectionTaskMtx;
    std::condition_variable m_waitForDetectionTask;
    std::thread m_detectorThread;

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
    std::vector<std::tuple<void*, OnDie>> m_dieListener;

    std::mutex m_saveVideoThreadsMtx;
    std::list<std::thread> m_saveVideoThreads;

    std::string m_videoDirectory;
};
