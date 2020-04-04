#pragma once

#include <string>
#include <set>
#include "HttpCommunication.h"
#include "SlackFileTypes.h"

class SlackCommunication
{
public:
    SlackCommunication(const std::string& address, const std::string& bearerId);
    ~SlackCommunication();

    using Channels = std::vector<std::string>;

    enum class JoinChannelAnswer
    {
        SUCCESS,
        ALREADY_JOINED,
        FAIL
    };
    JoinChannelAnswer joinChannel(const std::string& channelName);

    enum class SendAnswer
    {
        SUCCESS,
        CHANNEL_NOT_EXIST,
        FAIL
    };
    SendAnswer sendMessage(const std::string& channelName, const std::string& text);
    bool sendWelcomMessage(const std::string& channelName);
    SendAnswer sendFile(const Channels& channelNames, const std::string& filepath, SlackFileType fileType = SlackFileType::autoType);
    SendAnswer sendFile(const Channels& channelNames, const std::vector<char>& data, const std::string& filename, SlackFileType fileType);

private:
    std::string m_address;
    std::string m_bearerId;
    HttpCommunication m_http;

    HttpCommunication::HeaderList m_jsonHeaders;
    HttpCommunication::HeaderList m_multipartHeaders;
    std::string m_joinChannelAdr;
    std::string m_chatPostAdr;
    std::string m_fileUploadAdr;

    std::set<std::string> m_alreadyJoinedChannels;
};
