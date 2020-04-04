#include "SlackSubscriber.h"
#include "PngTools.h"

#include <iostream>

SlackSubscriber::SlackSubscriber(const Config &cfg)
    : m_cfg(cfg)
{
    m_notifyChannels.push_back(cfg.getValue("slackReportChannel", "general"));
    m_thread = std::thread(threadLoop, std::ref(*this));
}

SlackSubscriber::~SlackSubscriber()
{
    m_runThread = false;
    m_queueCv.notify_all();
    m_thread.join();
    unsubscribe();
}

void SlackSubscriber::subscribe(FrameControler &frameControler)
{
    if (m_frameControler) {
        std::cout << "Already subscribed!\n";
        return;
    }

    m_frameControler = &frameControler;
    m_frameControler->subscribeOnDetection(onDetect, this);
    m_frameControler->subscribeOnCurrentFrame(onCurrentFrameReady, this);
    m_frameControler->subscribeOnDetectionVideoReady(onVideoReady, this);
    m_frameControler->subscribeOnDie(handleDieingFrameControler, this);
}

void SlackSubscriber::unsubscribe()
{
    if (!m_frameControler)
        return;

    m_frameControler->unsubscribeOnDetection(onDetect, this);
    m_frameControler->unsubscribeOnCurrentFrame(onCurrentFrameReady, this);
    m_frameControler->unsubscribeOnDetectionVideoReady(onVideoReady, this);
    m_frameControler->unsubscribeOnDie(handleDieingFrameControler, this);
    m_frameControler = nullptr;
}

void SlackSubscriber::onCurrentFrameReady(const Frame &f, const FrameDescr &fd, void *ctx)
{
    SlackSubscriber& obj = *reinterpret_cast<SlackSubscriber*>(ctx);

    {
        std::unique_ptr<FrameData> frameData = std::unique_ptr<FrameData>(new FrameData{f, fd});
        std::lock_guard<std::mutex> lg(obj.m_currentFrameQueueMtx);
        obj.m_currentFrameQueue.push(std::move(frameData));
    }

    std::unique_lock<std::mutex> ul(obj.m_queueMtx);
    obj.m_queueTask.push(Task::FrameReady);
    obj.m_queueCv.notify_one();
}

void SlackSubscriber::onDetect(const Frame &f, const FrameDescr &fd, const std::string &detectionInfo, void *ctx)
{
    SlackSubscriber& obj = *reinterpret_cast<SlackSubscriber*>(ctx);

    {
        std::unique_ptr<DetectionData> detectionData = std::unique_ptr<DetectionData>(new DetectionData{{f, fd}, detectionInfo});
        std::lock_guard<std::mutex> lg(obj.m_detectionQueueMtx);
        obj.m_detectionQueue.push(std::move(detectionData));
    }

    std::unique_lock<std::mutex> ul(obj.m_queueMtx);
    obj.m_queueTask.push(Task::Detection);
    obj.m_queueCv.notify_one();
}

void SlackSubscriber::onVideoReady(const std::string &filePath, void *ctx)
{
    SlackSubscriber& obj = *reinterpret_cast<SlackSubscriber*>(ctx);

    {
        std::unique_ptr<std::string> videoFilePath = std::unique_ptr<std::string>(new std::string(filePath));
        std::lock_guard<std::mutex> lg(obj.m_videoQueueMtx);
        obj.m_videoQueue.push(std::move(videoFilePath));
    }

    std::unique_lock<std::mutex> ul(obj.m_queueMtx);
    obj.m_queueTask.push(Task::VideoReady);
    obj.m_queueCv.notify_one();
}

void SlackSubscriber::handleDieingFrameControler(void *ctx)
{
    SlackSubscriber& obj = *reinterpret_cast<SlackSubscriber*>(ctx);
    obj.unsubscribe();
}

void SlackSubscriber::threadLoop(SlackSubscriber& obj)
{
    std::cout << "Started slack thread\n";

    obj.m_slack = std::make_unique<SlackCommunication>(obj.m_cfg.getValue("slackAddress"), obj.m_cfg.getValue("slackBearerId"));
    if (!obj.m_slack->sendWelcomMessage(obj.m_notifyChannels[0])) {
        std::cerr << "Slack is not ready and can't be used!\n";
    }


    while(obj.m_runThread)
    {
        Task task = Task::Unknown;
        {
            std::unique_lock<std::mutex> ul(obj.m_queueMtx);
            obj.m_queueCv.wait(ul, [&obj]() {
                return !obj.m_queueTask.empty() || !obj.m_runThread;
            });
            if (!obj.m_runThread) {
                break;
            }
            task = obj.m_queueTask.front();
            obj.m_queueTask.pop();
        }

        switch (task) {
        case Task::FrameReady:
            obj.sendFrame();
            break;
        case Task::Detection:
            obj.sendDetectionData();
            break;
        case Task::VideoReady:
            obj.sendVideo();
            break;
        default:
            std::cerr << "Unkown task: " << static_cast<int>(task) << "\n";
            break;
        }
    }

    std::cout << "Finishing slack thread\n";
    std::cout.flush();
}

void SlackSubscriber::sendFrame()
{
    std::cout << "sendFrame\n";
    std::cout.flush();

    std::unique_ptr<FrameData> frameData;
    {
        std::lock_guard<std::mutex> lg(m_currentFrameQueueMtx);
        if (m_currentFrameQueue.empty()) {
            std::cerr << "Fake invoke. There is no frame to send!\n";
            return;
        }
        frameData.swap(m_currentFrameQueue.front());
        m_currentFrameQueue.pop();
    }
    std::string frameName = std::string(u8"frame_") + std::to_string(frameData->f.nr) + u8".png";
    m_slack->sendMessage(m_notifyChannels[0], std::string(u8"Frame ready to send :) ") + frameName);


    uint32_t rawSize = frameData->fd.width * frameData->fd.height * frameData->fd.components;
    uint32_t abc = PngTools::PNG_HEADER_SIZE;
    std::vector<char> pngFile(abc + rawSize, '\0');
    FILE* fd = fmemopen(pngFile.data(), pngFile.size(), "wb");
    if (PngTools::writePngFile(fd, frameData->fd.width, frameData->fd.height, frameData->fd.components, frameData->f.data.data())) {
        fclose(fd);
        std::cout << "abc " << abc << " aaaaaa  " << pngFile.size() << " next F_tell: " << ftell(fd) << "\n";
        std::cout.flush();

        pngFile.resize(static_cast<uint32_t>(ftell(fd)));
        m_slack->sendFile(m_notifyChannels, pngFile, frameName, SlackFileType::png);
    }
    else  {
        std::cerr << "Can't prepare png in memory!\n";
        fclose(fd);
    }
}

void SlackSubscriber::sendDetectionData()
{
    std::cout << "sendDetectionData\n";
    std::cout.flush();

    std::unique_ptr<DetectionData> detectionData;
    {
        std::lock_guard<std::mutex> lg(m_detectionQueueMtx);
        if (m_detectionQueue.empty()) {
            std::cerr << "Fake invoke. There is no detection to send!\n";
            return;
        }
        detectionData.swap(m_detectionQueue.front());
        m_detectionQueue.pop();
    }

    m_slack->sendMessage(m_notifyChannels[0], detectionData->detectionInfo);
    // TODO send frame
}

void SlackSubscriber::sendVideo()
{
    std::cout << "sendVideo\n";
    std::cout.flush();
    std::unique_ptr<std::string> videoFilePath;
    {
        std::lock_guard<std::mutex> lg(m_videoQueueMtx);
        if (m_videoQueue.empty()) {
            std::cerr << "Fake invoke. There is no video to send!\n";
            return;
        }
        videoFilePath.swap(m_videoQueue.front());
        m_videoQueue.pop();
    }

    m_slack->sendMessage(m_notifyChannels[0], std::string(u8"Sending: ") + *videoFilePath);
    m_slack->sendFile(m_notifyChannels, *videoFilePath);
}