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
const std::string MULTIPART_FORM_DATA_HEADER {"Content-type: multipart/form-data"};


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

    std::string authorizationHeader = std::string("Authorization: Bearer ") + m_bearerId;
    m_jsonHeaders.push_back(JSON_HEADER);
    m_jsonHeaders.push_back(authorizationHeader);

    m_multipartHeaders.push_back(MULTIPART_FORM_DATA_HEADER);
    m_multipartHeaders.push_back(authorizationHeader);

    m_joinChannelAdr = m_address + "channels.join";
    m_chatPostAdr = m_address + "chat.postMessage";
    m_fileUploadAdr = m_address + "files.upload";
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

    auto respond = m_http.post(m_joinChannelAdr, m_jsonHeaders, sb.GetString());
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


    auto respond = m_http.post(m_chatPostAdr, m_jsonHeaders, sb.GetString());
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

SlackCommunication::SendAnswer SlackCommunication::sendFile(const Channels &channelNames, const std::string &filepath, SlackFileType fileType)
{
    // token            Required
    // channels         Optional   Comma-separated list of channel names or IDs where the file will be shared.
    // content          Optional   File contents via a POST variable. If omitting this parameter, you must provide a file.
    // file             Optional   File contents via multipart/form-data. If omitting this parameter, you must submit content.
    // filename         Optional   Filename of file.
    // filetype         Optional   A file type identifier.
    // initial_comment  Optional   The message text introducing the file in specified channels.
    // thread_ts        Optional   Provide another message's ts value to upload this file as a reply. Never use a reply's ts value; use its parent instead.
    // title            Optional   Title of file.

    std::string channels = channelNames.empty() ? "general" : channelNames[0];
    for (uint32_t i = 1; i < channelNames.size(); ++i) {
        channels += ',';
        channels += channelNames[i];
    }

    HttpCommunication::Mimes mimes;
    HttpCommunication::Mime file;
    file.name = "file";
    file.filepath = filepath;
    if (fileType != SlackFileType::autoType) {
        file.fileType = slackFileTypeStr(fileType);
    }
    mimes.push_back(std::move(file));

    HttpCommunication::Mime channelsMime;
    channelsMime.name = "channels";
    channelsMime.data = std::vector<char>(channels.begin(), channels.end());
    mimes.push_back(std::move(channelsMime));

    auto respond = m_http.post(m_fileUploadAdr, m_multipartHeaders, "", mimes);
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

SlackCommunication::SendAnswer SlackCommunication::sendFile(const SlackCommunication::Channels &channelNames, const std::vector<char> &data, const std::string &filename, SlackFileType fileType)
{
    // token            Required
    // channels         Optional   Comma-separated list of channel names or IDs where the file will be shared.
    // content          Optional   File contents via a POST variable. If omitting this parameter, you must provide a file.
    // file             Optional   File contents via multipart/form-data. If omitting this parameter, you must submit content.
    // filename         Optional   Filename of file.
    // filetype         Optional   A file type identifier.
    // initial_comment  Optional   The message text introducing the file in specified channels.
    // thread_ts        Optional   Provide another message's ts value to upload this file as a reply. Never use a reply's ts value; use its parent instead.
    // title            Optional   Title of file.

    std::string channels = channelNames.empty() ? "general" : channelNames[0];
    for (uint32_t i = 1; i < channelNames.size(); ++i) {
        channels += ',';
        channels += channelNames[i];
    }

    HttpCommunication::Mimes mimes;
    HttpCommunication::Mime file;
    file.name = "file";
    file.data = data;
    file.fileName = filename;
    file.fileType = slackFileTypeStr(fileType);
    mimes.push_back(std::move(file));

    HttpCommunication::Mime channelsMime;
    channelsMime.name = "channels";
    channelsMime.data = std::vector<char>(channels.begin(), channels.end());
    mimes.push_back(std::move(channelsMime));

    auto respond = m_http.post(m_fileUploadAdr, m_multipartHeaders, "", mimes);
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
