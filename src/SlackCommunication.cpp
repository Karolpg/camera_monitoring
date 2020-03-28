#include "SlackCommunication.h"
#include <iostream>
#include <assert.h>
#include <sstream>

#include <rapidjson/document.h>
#include <rapidjson/prettywriter.h>
#include <rapidjson/error/en.h>

namespace  {
const std::string URL_ENC_HEADER {"Content-Type: application/x-www-form-urlencoded"};
const std::string JSON_HEADER {"Content-type: application/json; charset=utf-8"};


bool isTrue(const std::string& key, const rapidjson::Document& jsonDoc)
{
    auto ok = jsonDoc.FindMember(key.c_str());
    if (ok == jsonDoc.MemberEnd()) {
        std::cerr << "Error, JSON does not contain 'ok' key.\n";
        return false;
    }
    if (!ok->value.GetBool()) {
        std::cerr << "Error, JSON 'ok' is false.\n";
        return false;
    }
    return true;
}

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
    if (m_alreadyJoinedChannels.find(channelName) != m_alreadyJoinedChannels.end()) {
        return JoinChannelAnswer::ALREADY_JOINED;
    }

    rapidjson::StringBuffer sb;
    rapidjson::Writer<rapidjson::StringBuffer> jsonWriter(sb);
    jsonWriter.StartObject();

    static const std::string kName { u8"name" };
    jsonWriter.Key(kName.c_str(), static_cast<rapidjson::SizeType>(kName.length()));
    jsonWriter.String(channelName.c_str(), static_cast<rapidjson::SizeType>(channelName.length()));

    jsonWriter.EndObject();

    auto respond = m_http.post(m_joinChannelAdr, m_headers, sb.GetString());
    if (respond.errorMsg.has_value()) {
        return JoinChannelAnswer::FAIL;
    }
    assert(respond.serverAnswer.has_value());
    std::string& serverRespond = respond.serverAnswer.value();

    //std::cout << "\n\nServer respond is: " << serverRespond << "\n";

    rapidjson::Document jsonDoc;
    jsonDoc.Parse(serverRespond.c_str(), static_cast<rapidjson::SizeType>(serverRespond.length()));
    if (jsonDoc.HasParseError()) {
        rapidjson::ParseErrorCode error = jsonDoc.GetParseError();
        std::cerr << "Error during parsing JSON: " << error << " (" << rapidjson::GetParseError_En(error) << ")\n";
        std::cerr << "Server respond is: " << serverRespond << "\n";
        return JoinChannelAnswer::FAIL;
    }

    if (!isTrue("ok", jsonDoc)) {
        std::cerr << "Server respond is: " << serverRespond << "\n";
        return JoinChannelAnswer::FAIL;
    }
    m_alreadyJoinedChannels.insert(channelName);

    if (isTrue("already_in_channel", jsonDoc)) {
        return JoinChannelAnswer::ALREADY_JOINED;
    }

    return JoinChannelAnswer::SUCCESS;
}

SlackCommunication::SendAnswer SlackCommunication::sendMessage(const std::string &channelName, const std::string &text)
{
    rapidjson::StringBuffer sb;
    rapidjson::Writer<rapidjson::StringBuffer> jsonWriter(sb);
    jsonWriter.StartObject();

    static const std::string kChannel { u8"channel" };
    jsonWriter.Key(kChannel.c_str(), static_cast<rapidjson::SizeType>(kChannel.length()));
    jsonWriter.String(channelName.c_str(), static_cast<rapidjson::SizeType>(channelName.length()));

    static const std::string kText { u8"text" };
    jsonWriter.Key(kText.c_str(), static_cast<rapidjson::SizeType>(kText.length()));
    jsonWriter.String(text.c_str(), static_cast<rapidjson::SizeType>(text.length()));

    jsonWriter.EndObject();


    auto respond = m_http.post(m_chatPostAdr, m_headers, sb.GetString());
    if (respond.errorMsg.has_value()) {
        return SendAnswer::FAIL;
    }
    assert(respond.serverAnswer.has_value());
    std::string& serverRespond = respond.serverAnswer.value();

    //std::cout << "\n\nServer respond is: " << serverRespond << "\n";

    rapidjson::Document jsonDoc;
    jsonDoc.Parse(serverRespond.c_str(), static_cast<rapidjson::SizeType>(serverRespond.length()));
    if (jsonDoc.HasParseError()) {
        rapidjson::ParseErrorCode error = jsonDoc.GetParseError();
        std::cerr << "Error during parsing JSON: " << error << " (" << rapidjson::GetParseError_En(error) << ")\n";
        std::cerr << "Server respond is: " << serverRespond << "\n";
        return SendAnswer::FAIL;
    }
    if (!isTrue("ok", jsonDoc)) {
        std::cerr << "Server respond is: " << serverRespond << "\n";
        return SendAnswer::FAIL;
    }

    return SendAnswer::SUCCESS;
}


bool SlackCommunication::sendWelcomMessage(const std::string& channelName)
{
    static const char* welcomeMsg = u8"Awesome! We have just started Camera Monitoring application.\n";

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
