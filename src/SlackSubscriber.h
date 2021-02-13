//
// The MIT License (MIT)
//
// Copyright 2020 Karolpg
//
// Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), 
// to deal in the Software without restriction, including without limitation the rights to #use, copy, modify, merge, publish, distribute, sublicense, 
// and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, 
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR #COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, 
// WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
//

#pragma once

#include "SlackCommunication.h"
#include "FrameController.h"
#include "Config.h"
#include "Timer.h"

#include <thread>
#include <mutex>
#include <atomic>
#include <chrono>
#include <optional>
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
        CheckMsg,
        GatherMessages,
        DeleteMessages
    };

    struct MessageInfo {
        std::string channelId;
        std::string timeStamp;
    };

    struct DeleteMessagesRequest {
        std::optional<bool> all = false; // #1 - try to remove from newstTimePoint - to the end of available
        std::optional<uint32_t> count;   // #2 - try to remove form newstTimePoint - to count
        std::optional<std::chrono::system_clock::time_point> oldestTimePoint; // #3 - try to remove form newstTimePoint - oldestTimePoint
        std::chrono::system_clock::time_point newestTimePoint;
        std::string channelId;
    };

private:
    static void onCurrentFrameReady(const FrameU8& f, const FrameDescr& fd, void* ctx);
    static void onDetect(const FrameU8& f, const FrameDescr& fd, const std::string& detectionInfo, void* ctx);
    static void onVideoReady(const std::string& filePath, void* ctx);

    static void handleDieingFrameControler(void* ctx);

    static void threadLoop(SlackSubscriber& obj);

private:
    struct FrameData {
        FrameU8 f;
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

    void scheduleMsgChecking();
    void checkIncomingMessage();

    void gatheredMessagesToDelete();
    void deleteGathered();

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

    Timer<std::function<void()>> m_periodicFrameSender;


    Timer<std::function<void()>> m_periodicMessageChecker;
    std::atomic_bool m_allowScheduleCheckAgain; // blocking to add another check to queue in case we don't have time to proces previous request
    std::vector<std::chrono::system_clock::time_point> m_lastCheckedTimeStamp; // we don't want to process old request

    std::unique_ptr<DeleteMessagesRequest> m_deleteRequest;
    std::list<MessageInfo> m_messagesToDelete;
};
