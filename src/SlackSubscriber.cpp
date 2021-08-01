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
#include <algorithm>


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
            + u8" [MB]. If you want to download respond with: #giveVideo last";

    for (size_t c = 0; c < m_notifyChannels.size(); ++c) {
        m_slack->sendMessage(m_notifyChannels[c].name, text);
    }

    //m_slack->sendMessage(m_notifyChannels[0], std::string(u8"Sending: ") + *videoFilePath);
    //m_slack->sendFile(m_notifyChannels, *videoFilePath);
    //m_slack->sendFile(m_notifyChannels, *videoFilePath, SlackFileType::mpg);
    //Always send it as binary file!!!
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

        #define TEXT_AND_SIZE(txt) txt, sizeof(txt)-1

        if (StringUtils::starts_with(text, TEXT_AND_SIZE("#help"))) {
            static const std::string helpText =
                    "Camera monitoring supports:\n"
                    "  #help \n"
                    "  #giveFrame \n"
                    "  #delete {last [n], yyyy.mm.dd, all} \n"
                    "  #listVideo [all]\n"
                    "  #giveVideo {last, nr n, filename}";
            m_slack->sendMessage(m_notifyChannels[c].name, helpText);
        }
        else if (StringUtils::starts_with(text, TEXT_AND_SIZE("#giveFrame"))) {
            if (m_frameControler) {
                m_frameControler->subscribeOnCurrentFrame(onCurrentFrameReady, this);
            }
        }
        else if (StringUtils::starts_with(text, TEXT_AND_SIZE("#delete"))) {
            if (m_deleteRequest) {
                m_slack->sendMessage(m_notifyChannels[c].name, "Currently processing another delete request!");
            }
            else {
                uint32_t afterDelete = sizeof("#delete"); // null is like space after :)
                std::string cmd = text.length() >= afterDelete ? text.substr(afterDelete) : std::string();
                handleDeleteSubCmd(cmd, currentMsgTs, c);
            }
        }
        else if (StringUtils::starts_with(text, TEXT_AND_SIZE("#listVideo"))) {
            uint32_t afterListVideos = sizeof("#listVideo"); // null is like space after :)
            std::string cmd = text.length() >= afterListVideos ? text.substr(afterListVideos) : std::string();
            handleListVideos(cmd, c);
        }
        else if (StringUtils::starts_with(text, TEXT_AND_SIZE("#giveVideo"))) {
            uint32_t afterGiveVideo = sizeof("#giveVideo"); // null is like space after :)
            std::string cmd = text.length() >= afterGiveVideo ? text.substr(afterGiveVideo) : std::string();
            handleGiveVideo(cmd, c);
        }
        #undef TEXT_AND_SIZE
    }
}

void SlackSubscriber::handleListVideos(const std::string& subcmd, size_t channel)
{
    std::string videoDirectory = DirUtils::cleanPath(m_cfg.getValue("videoStorePath", "/tmp"));
    std::vector<std::string> videosList;
    {
        auto dirContent = DirUtils::listDir(videoDirectory);
        const std::string extension = ".mpeg";
        for (const auto& entry : dirContent) {
            if (StringUtils::ends_with(entry, extension)) {
                videosList.push_back(entry);
            }
        }
    }
    std::sort(videosList.begin(), videosList.end(), std::greater<std::string>());
    auto lastToSend = videosList.end();
    if (subcmd != "all") {
        const size_t VIDEO_LIMIT = 10;
        lastToSend = videosList.size() > VIDEO_LIMIT ? videosList.begin() + VIDEO_LIMIT : videosList.end();
    }
    std::string msgTxt;
    std::for_each(videosList.begin(), lastToSend, [&msgTxt](const std::string& entry) { msgTxt += entry; msgTxt += "\n"; } );
    if (msgTxt.empty()) {
        msgTxt = u8"There is no video files";
    }
    m_slack->sendMessage(m_notifyChannels[channel].name, msgTxt);
}

void SlackSubscriber::handleGiveVideo(const std::string& subcmd, size_t channel)
{
    std::string filename;
    int32_t idx = -1;
    #define TEXT_AND_SIZE(txt) txt, sizeof(txt)-1
    if (StringUtils::starts_with(subcmd, TEXT_AND_SIZE("last"))) {
        idx = 0;
    }
    else if (StringUtils::starts_with(subcmd, TEXT_AND_SIZE("nr"))) {
        uint32_t afterNr = sizeof("nr"); // null is like space after :)
        std::string number = subcmd.length() >= afterNr ? subcmd.substr(afterNr) : std::string();
        int32_t num = -1;
        try {
            num = number.empty() ? 1 : std::stoi(number);
        }
        catch(const std::exception& e) {
            std::string info = std::string("Invalid command. Can't convert str to number: ") + e.what() + "\n";
            std::cerr << info;
            m_slack->sendMessage(m_notifyChannels[channel].name, info);
            return;
        }
        if (num < 0) {
            std::string info =  "Invalid command. Negative number: " + number + "\n";
            std::cerr << info;
            m_slack->sendMessage(m_notifyChannels[channel].name, info);
            return;
        }
        idx = num;
    }

    std::string videoDirectory = DirUtils::cleanPath(m_cfg.getValue("videoStorePath", "/tmp"));
    if (idx >=0)
    {
        // list dir and take by index - zero means latest/newest
        std::vector<std::string> videosList;
        {
            auto dirContent = DirUtils::listDir(videoDirectory);
            const std::string extension = ".mpeg";
            for (const auto& entry : dirContent) {
                if (StringUtils::ends_with(entry, extension)) {
                    videosList.push_back(entry);
                }
            }
        }
        std::sort(videosList.begin(), videosList.end(), std::greater<std::string>());
        if (videosList.size() <= static_cast<size_t>(idx)) {
            std::string info = "Wrong video index. There is not enought videos. Video count: " + std::to_string(videosList.size())
                             + " and you have ordered: " + std::to_string(idx) + "\n";
            std::cerr << info;
            m_slack->sendMessage(m_notifyChannels[channel].name, info);
            return;
        }
        filename = videoDirectory + "/" + videosList[static_cast<size_t>(idx)];
    }
    else {
        filename = videoDirectory + "/" + subcmd;
        if (!DirUtils::isFile(filename)) {
            std::string info = "File you have ordered does not exists: " + subcmd + "\n";
            std::cerr << info;
            m_slack->sendMessage(m_notifyChannels[channel].name, info);
            return;
        }
    }
    m_slack->sendFile(SlackCommunication::Channels(1, m_notifyChannels[channel]), filename);

    #undef TEXT_AND_SIZE
}

void SlackSubscriber::handleDeleteSubCmd(const std::string& subcmd, const std::chrono::system_clock::time_point& currentMsgTs, size_t channel)
{
    if (subcmd == "all") {
        m_deleteRequest = std::make_unique<DeleteMessagesRequest>();
        m_deleteRequest->all = true;
        m_deleteRequest->newestTimePoint = currentMsgTs;
        m_deleteRequest->channelId = m_notifyChannels[channel].id;
    }
    else if (StringUtils::starts_with(subcmd, "last")) {
        uint32_t afterLast = sizeof("last"); // null is like space after :)
        std::string number = subcmd.length() >= afterLast ? subcmd.substr(afterLast) : std::string();
        int num = number.empty() ? 1 : std::stoi(number);
        if (num > 0) {
            m_deleteRequest = std::make_unique<DeleteMessagesRequest>();
            m_deleteRequest->count = static_cast<uint32_t>(num);
            m_deleteRequest->newestTimePoint = currentMsgTs;
            m_deleteRequest->channelId = m_notifyChannels[channel].id;
        }
    }
    else {
        TimeUtils::DateTime dt{0};
        if (TimeUtils::parseDateTime(subcmd, dt)) {
            m_deleteRequest = std::make_unique<DeleteMessagesRequest>();
            m_deleteRequest->oldestTimePoint = TimeUtils::createTimePoint(dt.year, dt.month, dt.day, 0, 0, 0);
            m_deleteRequest->newestTimePoint = TimeUtils::createTimePoint(dt.year, dt.month, dt.day, 23, 59, 59);
            m_deleteRequest->channelId = m_notifyChannels[channel].id;
        }
    }

    if (m_deleteRequest) {
        std::shared_ptr<Job> gatherMessagesJob = std::shared_ptr<Job>(new SimpleJob( [this]() { gatheredMessagesToDelete(); } ));
        m_slackThread.addJob(std::chrono::microseconds(0), gatherMessagesJob);
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
        gatheredAllMessagesToDelete();
        return;
    }


    if(m_deleteRequest->count.has_value()) {
        gatheredCountedMessagesToDelete();
        return;
    }

    if (m_deleteRequest->oldestTimePoint.has_value()) {
        gatheredRangedMessagesToDelete();
        return;
    }
}

void SlackSubscriber::gatheredAllMessagesToDelete()
{
    std::vector<SlackCommunication::Message> messages;

    auto result = m_slack->listChannelMessage(messages, m_deleteRequest->channelId, 100,
                                              std::string("0"), // from 1970
                                              TimeUtils::timePointToTimeStamp(m_deleteRequest->newestTimePoint));

    if (result == SlackCommunication::GeneralAnswer::RATE_LIMITED) {
        std::shared_ptr<Job> gatherMessagesJob = std::shared_ptr<Job>(new SimpleJob( [this]() { gatheredAllMessagesToDelete(); } ));
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

        std::shared_ptr<Job> gatherMessagesJob = std::shared_ptr<Job>(new SimpleJob( [this]() { gatheredAllMessagesToDelete(); } ));
        m_slackThread.addJob(std::chrono::microseconds(0), gatherMessagesJob);

        std::shared_ptr<Job> deleteMessagesJob = std::shared_ptr<Job>(new SimpleJob( [this]() { deleteGathered(); } ));
        m_slackThread.addJob(std::chrono::microseconds(0), deleteMessagesJob);
    }
}

void SlackSubscriber::gatheredCountedMessagesToDelete()
{
    std::vector<SlackCommunication::Message> messages;

    auto count = m_deleteRequest->count.value();

    if (count == 0){
        m_deleteRequest.reset();
        return;
    }

    auto result = m_slack->listChannelMessage(messages, m_deleteRequest->channelId, count,
                                              std::string("0"), // from 1970
                                              TimeUtils::timePointToTimeStamp(m_deleteRequest->newestTimePoint));

    if (result == SlackCommunication::GeneralAnswer::RATE_LIMITED) {
        std::shared_ptr<Job> gatherMessagesJob = std::shared_ptr<Job>(new SimpleJob( [this]() { gatheredCountedMessagesToDelete(); } ));
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

            std::shared_ptr<Job> gatherMessagesJob = std::shared_ptr<Job>(new SimpleJob( [this]() { gatheredCountedMessagesToDelete(); } ));
            m_slackThread.addJob(std::chrono::microseconds(0), gatherMessagesJob);
        }
        else {
            m_deleteRequest.reset();
        }

        std::shared_ptr<Job> deleteMessagesJob = std::shared_ptr<Job>(new SimpleJob( [this]() { deleteGathered(); } ));
        m_slackThread.addJob(std::chrono::microseconds(0), deleteMessagesJob);
    }
}

void SlackSubscriber::gatheredRangedMessagesToDelete()
{
    std::vector<SlackCommunication::Message> messages;

    auto oldestTimePoint = m_deleteRequest->oldestTimePoint.value();

    auto result = m_slack->listChannelMessage(messages, m_deleteRequest->channelId, 100,
                                              TimeUtils::timePointToTimeStamp(oldestTimePoint),
                                              TimeUtils::timePointToTimeStamp(m_deleteRequest->newestTimePoint));

    if (result == SlackCommunication::GeneralAnswer::RATE_LIMITED) {
        std::shared_ptr<Job> gatherMessagesJob = std::shared_ptr<Job>(new SimpleJob( [this]() { gatheredRangedMessagesToDelete(); } ));
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

        std::shared_ptr<Job> gatherMessagesJob = std::shared_ptr<Job>(new SimpleJob( [this]() { gatheredRangedMessagesToDelete(); } ));
        m_slackThread.addJob(std::chrono::microseconds(0), gatherMessagesJob);

        std::shared_ptr<Job> deleteMessagesJob = std::shared_ptr<Job>(new SimpleJob( [this]() { deleteGathered(); } ));
        m_slackThread.addJob(std::chrono::microseconds(0), deleteMessagesJob);
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
