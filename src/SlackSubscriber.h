#pragma once

#include "SlackCommunication.h"
#include "FrameControler.h"
#include "Config.h"

#include <thread>
#include <mutex>
#include <condition_variable>

class SlackSubscriber
{

public:
    SlackSubscriber(const Config& cfg);
    ~SlackSubscriber();

    void subscribe(FrameController& frameControler);
    void unsubscribe();

private:
    enum class Task {
        Unknown,
        FrameReady,
        Detection,
        VideoReady,
    };

private:
    static void onCurrentFrameReady(const Frame& f, const FrameDescr& fd, void* ctx);
    static void onDetect(const Frame& f, const FrameDescr& fd, const std::string& detectionInfo, void* ctx);
    static void onVideoReady(const std::string& filePath, void* ctx);

    static void handleDieingFrameControler(void* ctx);

    static void threadLoop(SlackSubscriber& obj);

private:
    struct FrameData {
        Frame f;
        FrameDescr fd;
    };
    std::queue<std::unique_ptr<FrameData>> m_currentFrameQueue;
    std::mutex m_currentFrameQueueMtx;
    void sendFrame();


    struct DetectionData {
        FrameData frameData;
        std::string detectionInfo;
    };
    std::queue<std::unique_ptr<DetectionData>> m_detectionQueue;
    std::mutex m_detectionQueueMtx;
    void sendDetectionData();

    std::queue<std::unique_ptr<std::string>> m_videoQueue;
    std::mutex m_videoQueueMtx;
    void sendVideo();

private:
    const Config &m_cfg;
    FrameController* m_frameControler = nullptr;
    std::unique_ptr<SlackCommunication> m_slack;
    SlackCommunication::Channels m_notifyChannels;
    std::vector<char> m_memoryPngFile;

    std::thread m_thread;
    std::mutex m_queueMtx;
    std::condition_variable m_queueCv;
    std::queue<Task> m_queueTask;
    volatile bool m_runThread = true;
};
