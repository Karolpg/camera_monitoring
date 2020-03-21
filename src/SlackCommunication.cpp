#include "SlackCommunication.h"
#include <iostream>
#include <assert.h>
#include <sstream>

namespace  {
const std::string URL_ENC_HEADER {"Content-Type: application/x-www-form-urlencoded"};
const std::string JSON_HEADER {"Content-type: application/json; charset=utf-8"};
} // namespace

SlackCommunication::SlackCommunication(const std::string &address, const std::string &bearerId)
    : m_address(address)
    , m_bearerId(bearerId)
    , m_http()
{
    if (m_address.empty()) {
        m_address = "https://slack.com/api/";
    }
    if (m_address.back() != '/') {
        m_address += '/';
    }

    m_headers.push_back(JSON_HEADER);
    m_headers.push_back(std::string("Authorization: Bearer ") + m_bearerId);
    m_joinChannelAdr = m_address + "channels.join";
    m_chatPostAdr = m_address + "chat.postMessage";
}

SlackCommunication::~SlackCommunication()
{

}

SlackCommunication::JoinChannelAnswer SlackCommunication::joinChannel(const std::string &channelName)
{
    std::stringstream data;
    data << u8"{\n";
    data << u8"\"name\":\"" << channelName << "\"";
    data << u8"}";

    auto respond = m_http.post(m_joinChannelAdr, m_headers, data.str());
    if (respond.errorMsg.has_value()) {
        return JoinChannelAnswer::FAIL;
    }
    assert(respond.serverAnswer.has_value());
    std::string& serverRespond = respond.serverAnswer.value();
    std::cout << "\n\nServer respond is: " << serverRespond << "\n";
    return JoinChannelAnswer::SUCCESS;
}

SlackCommunication::SendAnswer SlackCommunication::sendMessage(const std::string &channelName, const std::string &text)
{
    std::stringstream data;
    data << u8"{\n";
    data << u8"\"channel\":\"" << channelName << "\",";
    data << u8"\"text\":\"" << text << "\"";
    data << u8"}";

    auto respond = m_http.post(m_chatPostAdr, m_headers, data.str());
    if (respond.errorMsg.has_value()) {
        return SendAnswer::FAIL;
    }
    assert(respond.serverAnswer.has_value());
    std::string& serverRespond = respond.serverAnswer.value();
    std::cout << "\n\nServer respond is: " << serverRespond << "\n";
    return SendAnswer::SUCCESS;
}


bool SlackCommunication::sendWelcomMessage(const std::string& channelName)
{
    static const char* welcomeMsg = "Awesome! We have just started Camera Monitoring application.\n";

    if (joinChannel(channelName) != SlackCommunication::JoinChannelAnswer::FAIL) {
        if (sendMessage(channelName, welcomeMsg) == SlackCommunication::SendAnswer::FAIL) {
            std::cerr << "Can't send welcome message\n";
            return false;
        }
        return true;
    }
    std::cerr << "Can't join Slack channel: " << channelName << "\n";
    return false;
}
