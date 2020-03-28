#pragma once

#include <string>
#include <set>
#include "HttpCommunication.h"

class SlackCommunication
{
public:
    SlackCommunication(const std::string& address, const std::string& bearerId);
    ~SlackCommunication();

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
        FAIL
    };
    SendAnswer sendMessage(const std::string& channelName, const std::string& text);

    bool sendWelcomMessage(const std::string& channelName);

    // TODO: send image or even video

private:
    std::string m_address;
    std::string m_bearerId;
    HttpCommunication m_http;

    HttpCommunication::HeaderList m_headers;
    std::string m_joinChannelAdr;
    std::string m_chatPostAdr;

    std::set<std::string> m_alreadyJoinedChannels;
};
