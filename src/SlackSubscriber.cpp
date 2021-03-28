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

#include "SlackSubscriber.h"
#include "PngTools.h"
#include "TimeUtils.h"
#include "DirUtils.h"
#include "StringUtils.h"

#include <iostream>
#include <fstream>
#include <cstring>
#include <assert.h>


SlackSubscriber::SlackSubscriber(const Config &cfg)
    : m_cfg(cfg)
{
    //m_thread = std::thread(threadLoop, std::ref(*this));
    initSlack();

    int sendEvery = cfg.getValue("sendFrameByEverySeconds", -1);
    if (sendEvery > 0) {
        m_sendPeriodicFrameJob = std::shared_ptr<Job>(new SimpleJob( [this]() {   if (m_frameControler) {
                                                                                    m_frameControler->subscribeOnCurrentFrame(onCurrentFrameReady, this);
                                                                                  }
                                                                              },
                                                                            std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::seconds(sendEvery)) ));
        m_slackThread.addJob(std::chrono::microseconds(0), m_sendPeriodicFrameJob);
    }

    std::shared_ptr<Job> checkIncomingMsgJob = std::shared_ptr<Job>(new SimpleJob( [this]() { checkIncomingMessage(); }, std::chrono::microseconds(3* 1000 * 1000) ));
    m_slackThread.addJob(std::chrono::microseconds(0), checkIncomingMsgJob);
}

SlackSubscriber::~SlackSubscriber()
{
    unsubscribe();
    m_slackThread.removeAllJobs();
}

void SlackSubscriber::initSlack()
{
    m_slack = std::make_unique<SlackCommunication>(m_cfg.getValue("slackAddress"), m_cfg.getValue("slackBearerId"));
    {
        std::string channelName = m_cfg.getValue("slackReportChannel", "general");
        SlackCommunication::Channels channels;
        m_slack->listConversations(channels);
        for (const auto& channel : channels) {
            if (channel.name == channelName) {
                m_notifyChannels.push_back(channel);
                break;
            }
        }
    }

    for (size_t c = 0; c < m_notifyChannels.size(); ++c) {
        if (!m_slack->sendWelcomMessage(m_notifyChannels[c].name)) {
            std::cerr << "Slack is not ready and can't be used!\n";
        }
    }
}


void SlackSubscriber::subscribe(FrameController &frameControler)
{
    if (m_frameControler) {
        std::cout << "Already subscribed!\n";
        return;
    }

    m_frameControler = &frameControler;
    m_frameControler->subscribeOnDetection(onDetect, this);
    //m_frameControler->subscribeOnCurrentFrame(onCurrentFrameReady, this);
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

void SlackSubscriber::onCurrentFrameReady(const FrameU8 &f, const FrameDescr &fd, void *ctx)
{
    SlackSubscriber* localThis = reinterpret_cast<SlackSubscriber*>(ctx);
    assert(localThis);

    {
        std::unique_ptr<FrameData> frameData = std::unique_ptr<FrameData>(new FrameData{f, fd});
        std::lock_guard<std::mutex> lg(localThis->m_currentFrameQueueMtx);
        localThis->m_currentFrameQueue.push(std::move(frameData));
    }


    std::shared_ptr<Job> frameReadyJob = std::shared_ptr<Job>(new SimpleJob( [localThis]() { localThis->sendFrame(); } ));
    localThis->m_slackThread.addJob(std::chrono::microseconds(0), frameReadyJob);
}

void SlackSubscriber::onDetect(const FrameU8 &f, const FrameDescr &fd, const std::string &detectionInfo, void *ctx)
{
    SlackSubscriber* localThis = reinterpret_cast<SlackSubscriber*>(ctx);
    assert(localThis);

    {
        std::unique_ptr<DetectionData> detectionData = std::unique_ptr<DetectionData>(new DetectionData{{f, fd}, detectionInfo});
        std::lock_guard<std::mutex> lg(localThis->m_detectionQueueMtx);
        localThis->m_detectionQueue.push(std::move(detectionData));
    }

    std::shared_ptr<Job> detectJob = std::shared_ptr<Job>(new SimpleJob( [localThis]() { localThis->sendDetectionData(); } ));
    localThis->m_slackThread.addJob(std::chrono::microseconds(0), detectJob);
}

void SlackSubscriber::onVideoReady(const std::string &filePath, void *ctx)
{
    SlackSubscriber* localThis = reinterpret_cast<SlackSubscriber*>(ctx);
    assert(localThis);

    {
        std::unique_ptr<std::string> videoFilePath = std::unique_ptr<std::string>(new std::string(filePath));
        std::lock_guard<std::mutex> lg(localThis->m_videoQueueMtx);
        localThis->m_videoQueue.push(std::move(videoFilePath));
    }

    std::shared_ptr<Job> sendVideoJob = std::shared_ptr<Job>(new SimpleJob( [localThis]() { localThis->sendVideo(); } ));
    localThis->m_slackThread.addJob(std::chrono::microseconds(0), sendVideoJob);
}

void SlackSubscriber::handleDieingFrameControler(void *ctx)
{
    SlackSubscriber& obj = *reinterpret_cast<SlackSubscriber*>(ctx);
    obj.m_slackThread.removeJob(obj.m_sendPeriodicFrameJob);
    obj.unsubscribe();
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
    for (size_t c = 0; c < m_notifyChannels.size(); ++c) {
        m_slack->sendMessage(m_notifyChannels[c].name, std::string(u8"Frame ready to send :) ") + frameName);
    }


    uint32_t rawSize = frameData->fd.width * frameData->fd.height * frameData->fd.components;
    m_memoryPngFile.resize(PngTools::PNG_HEADER_SIZE + rawSize, '\0');
    FILE* fd = fmemopen(m_memoryPngFile.data(), m_memoryPngFile.size(), "wb");
    if (PngTools::writePngFile(fd, frameData->fd.width, frameData->fd.height, frameData->fd.components, frameData->f.data.data())) {
        fclose(fd);
        size_t fileSize = static_cast<size_t>(ftell(fd));
        m_slack->sendFile(m_notifyChannels, m_memoryPngFile.data(), fileSize, frameName, SlackFileType::png);
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

    for (size_t c = 0; c < m_notifyChannels.size(); ++c) {
        m_slack->sendMessage(m_notifyChannels[c].name, detectionData->detectionInfo);
    }

    const auto& frameData = detectionData->frameData;

    uint32_t rawSize = frameData.fd.width * frameData.fd.height * frameData.fd.components;
    m_memoryPngFile.resize(PngTools::PNG_HEADER_SIZE + rawSize, '\0');
    FILE* fd = fmemopen(m_memoryPngFile.data(), m_memoryPngFile.size(), "wb");
    if (PngTools::writePngFile(fd, frameData.fd.width, frameData.fd.height, frameData.fd.components, frameData.f.data.data())) {
        fclose(fd);
        size_t fileSize = static_cast<size_t>(ftell(fd));
        std::string frameName = std::string(u8"frame_") + std::to_string(frameData.f.nr) + u8".png";
        m_slack->sendFile(m_notifyChannels, m_memoryPngFile.data(), fileSize, frameName, SlackFileType::png);
    }
    else  {
        std::cerr << "Can't prepare png in memory!\n";
        fclose(fd);
    }
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

    double videoSize = DirUtils::file_size(*videoFilePath);
    //double videoSize = std::filesystem::file_size(*videoFilePath);
    videoSize /= (1024.0 * 1024.0);

    std::string text = std::string(u8"Video: ") + *videoFilePath + u8" is ready to send. Size: " + std::to_string(videoSize)
            + u8" [MB]. If you want to download respond with: ... - this is not ready!!!.";

    for (size_t c = 0; c < m_notifyChannels.size(); ++c) {
        m_slack->sendMessage(m_notifyChannels[c].name, text);
    }

    //m_slack->sendMessage(m_notifyChannels[0], std::string(u8"Sending: ") + *videoFilePath);
    //m_slack->sendFile(m_notifyChannels, *videoFilePath);
}

void SlackSubscriber::checkIncomingMessage()
{
    SlackCommunication::Message msg;
    m_lastCheckedTimeStamp.resize(m_notifyChannels.size());
    for (size_t c = 0; c < m_notifyChannels.size(); ++c) {
        auto result = m_slack->lastChannelMessage(m_notifyChannels[c].id, msg);
        if (result != SlackCommunication::GeneralAnswer::SUCCESS) {
            continue;
        }

        auto currentMsgTs = TimeUtils::timeStampToTimePoint(msg.timeStamp);
        if (m_lastCheckedTimeStamp[c] >= currentMsgTs) {
            continue; //already processed
        }
        m_lastCheckedTimeStamp[c] = currentMsgTs;

        if (!msg.text.has_value())
            continue;
        const std::string& text = msg.text.value();
        if (text.empty() || text.front() != '#') {
            continue; // doesn't contain special character at the begining
        }

        if (StringUtils::starts_with(text, "#help", 5)) {
            static const std::string helpText =
                    "Camera monitoring supports:\n"
                    "  #help \n"
                    "  #giveFrame \n"
                    "  #delete {last [n], yyyy.mm.dd, all} \n";
            m_slack->sendMessage(m_notifyChannels[c].name, helpText);
        }
        else if (StringUtils::starts_with(text, "#giveFrame", 10)) {
            if (m_frameControler) {
                m_frameControler->subscribeOnCurrentFrame(onCurrentFrameReady, this);
            }
        }
        else if (StringUtils::starts_with(text, "#delete", 7)) {
            if (m_deleteRequest) {
                m_slack->sendMessage(m_notifyChannels[c].name, "Currently processing another delete request!");
            }
            else {
                std::string cmd = text.length() >= 8 ? text.substr(8) : std::string();
                if (cmd == "all") {
                    m_deleteRequest = std::make_unique<DeleteMessagesRequest>();
                    m_deleteRequest->all = true;
                    m_deleteRequest->newestTimePoint = currentMsgTs;
                    m_deleteRequest->channelId = m_notifyChannels[c].id;
                }
                else if (StringUtils::starts_with(cmd, "last")) {
                    std::string number = cmd.length() >= 5 ? cmd.substr(5) : std::string();
                    int num = number.empty() ? 1 : std::stoi(number);
                    if (num > 0) {
                        m_deleteRequest = std::make_unique<DeleteMessagesRequest>();
                        m_deleteRequest->count = static_cast<uint32_t>(num);
                        m_deleteRequest->newestTimePoint = currentMsgTs;
                        m_deleteRequest->channelId = m_notifyChannels[c].id;
                    }
                }
                else {
                    TimeUtils::DateTime dt{0};
                    if (TimeUtils::parseDateTime(cmd, dt)) {
                        m_deleteRequest = std::make_unique<DeleteMessagesRequest>();
                        m_deleteRequest->oldestTimePoint = TimeUtils::createTimePoint(dt.year, dt.month, dt.day, 0, 0, 0);
                        m_deleteRequest->newestTimePoint = TimeUtils::createTimePoint(dt.year, dt.month, dt.day, 23, 59, 59);
                        m_deleteRequest->channelId = m_notifyChannels[c].id;
                    }
                }

                if (m_deleteRequest) {
                    std::shared_ptr<Job> gatherMessagesJob = std::shared_ptr<Job>(new SimpleJob( [this]() { gatheredMessagesToDelete(); } ));
                    m_slackThread.addJob(std::chrono::microseconds(0), gatherMessagesJob);
                }
            }
        }
    }
}

void SlackSubscriber::gatheredMessagesToDelete()
{
    if (!m_deleteRequest) {
        return;
    }
    std::vector<SlackCommunication::Message> messages;

    bool isAll = m_deleteRequest->all.has_value() && m_deleteRequest->all.value() ? true : false;
    if (isAll) {
        auto result = m_slack->listChannelMessage(messages, m_deleteRequest->channelId, 100,
                                                  std::string("0"), // from 1970
                                                  TimeUtils::timePointToTimeStamp(m_deleteRequest->newestTimePoint));

        if (result == SlackCommunication::GeneralAnswer::RATE_LIMITED) {
            std::shared_ptr<Job> gatherMessagesJob = std::shared_ptr<Job>(new SimpleJob( [this]() { gatheredMessagesToDelete(); } ));
            m_slackThread.addJob(std::chrono::microseconds(1000 * 1000), gatherMessagesJob); // 1s delay - wait for another chance
            return;
        }

        if (messages.empty()) {
            m_deleteRequest.reset();
        }
        else {
            for (size_t i = 0; i < messages.size(); ++i) {
                // todo add selecting bot messages
                m_messagesToDelete.push_back({m_deleteRequest->channelId, messages[i].timeStamp});
            }
            m_deleteRequest->newestTimePoint = TimeUtils::timeStampToTimePoint(messages.back().timeStamp); // do not take already taken

            std::shared_ptr<Job> gatherMessagesJob = std::shared_ptr<Job>(new SimpleJob( [this]() { gatheredMessagesToDelete(); } ));
            m_slackThread.addJob(std::chrono::microseconds(0), gatherMessagesJob);

            std::shared_ptr<Job> deleteMessagesJob = std::shared_ptr<Job>(new SimpleJob( [this]() { deleteGathered(); } ));
            m_slackThread.addJob(std::chrono::microseconds(0), deleteMessagesJob);
        }
        return;
    }


    if(m_deleteRequest->count.has_value()) {
        auto count = m_deleteRequest->count.value();

        if (count == 0){
            m_deleteRequest.reset();
            return;
        }

        auto result = m_slack->listChannelMessage(messages, m_deleteRequest->channelId, count,
                                                  std::string("0"), // from 1970
                                                  TimeUtils::timePointToTimeStamp(m_deleteRequest->newestTimePoint));

        if (result == SlackCommunication::GeneralAnswer::RATE_LIMITED) {
            std::shared_ptr<Job> gatherMessagesJob = std::shared_ptr<Job>(new SimpleJob( [this]() { gatheredMessagesToDelete(); } ));
            m_slackThread.addJob(std::chrono::microseconds(1000 * 1000), gatherMessagesJob); // 1s delay - wait for another chance
            return;
        }

        if (messages.empty()) {
            m_deleteRequest.reset();
        }
        else {
            for (size_t i = 0; i < messages.size(); ++i) {
                // todo add selecting bot messages
                m_messagesToDelete.push_back({m_deleteRequest->channelId, messages[i].timeStamp});
            }
            assert(messages.size() <= count);
            count = count - messages.size();
            m_deleteRequest->count = count; // take rest if any
            if (count) {
                m_deleteRequest->newestTimePoint = TimeUtils::timeStampToTimePoint(messages.back().timeStamp); // do not take already taken

                std::shared_ptr<Job> gatherMessagesJob = std::shared_ptr<Job>(new SimpleJob( [this]() { gatheredMessagesToDelete(); } ));
                m_slackThread.addJob(std::chrono::microseconds(0), gatherMessagesJob);
            }
            else {
                m_deleteRequest.reset();
            }

            std::shared_ptr<Job> deleteMessagesJob = std::shared_ptr<Job>(new SimpleJob( [this]() { deleteGathered(); } ));
            m_slackThread.addJob(std::chrono::microseconds(0), deleteMessagesJob);
        }
        return;
    }

    if (m_deleteRequest->oldestTimePoint.has_value()) {
        auto oldestTimePoint = m_deleteRequest->oldestTimePoint.value();

        auto result = m_slack->listChannelMessage(messages, m_deleteRequest->channelId, 100,
                                                  TimeUtils::timePointToTimeStamp(oldestTimePoint),
                                                  TimeUtils::timePointToTimeStamp(m_deleteRequest->newestTimePoint));

        if (result == SlackCommunication::GeneralAnswer::RATE_LIMITED) {
            std::shared_ptr<Job> gatherMessagesJob = std::shared_ptr<Job>(new SimpleJob( [this]() { gatheredMessagesToDelete(); } ));
            m_slackThread.addJob(std::chrono::microseconds(1000 * 1000), gatherMessagesJob); // 1s delay - wait for another chance
            return;
        }

        if (messages.empty()) {
            m_deleteRequest.reset();
        }
        else {
            for (size_t i = 0; i < messages.size(); ++i) {
                // todo add selecting bot messages
                m_messagesToDelete.push_back({m_deleteRequest->channelId, messages[i].timeStamp});
            }
            m_deleteRequest->newestTimePoint = TimeUtils::timeStampToTimePoint(messages.back().timeStamp); // do not take already taken

            std::shared_ptr<Job> gatherMessagesJob = std::shared_ptr<Job>(new SimpleJob( [this]() { gatheredMessagesToDelete(); } ));
            m_slackThread.addJob(std::chrono::microseconds(0), gatherMessagesJob);

            std::shared_ptr<Job> deleteMessagesJob = std::shared_ptr<Job>(new SimpleJob( [this]() { deleteGathered(); } ));
            m_slackThread.addJob(std::chrono::microseconds(0), deleteMessagesJob);
        }
        return;
    }
}

void SlackSubscriber::deleteGathered()
{
    if (m_messagesToDelete.empty()) {
        return;
    }

    for (auto it = m_messagesToDelete.begin(); it != m_messagesToDelete.end(); ) {
        auto result = m_slack->deleteMessageChannel(it->channelId, it->timeStamp);
        if (result == SlackCommunication::GeneralAnswer::RATE_LIMITED) {

            std::shared_ptr<Job> deleteMessagesJob = std::shared_ptr<Job>(new SimpleJob( [this]() { deleteGathered(); } ));
            m_slackThread.addJob(std::chrono::microseconds(1000 * 1000), deleteMessagesJob); // 1s delay - wait for another chance
            return;
        }

        if (result != SlackCommunication::GeneralAnswer::SUCCESS) {
            std::cerr << "Can't remove message channel: " << it->channelId << " ts: " << it->timeStamp << "\n";
        }
        it = m_messagesToDelete.erase(it);
    }
}
