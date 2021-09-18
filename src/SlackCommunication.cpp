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

#include "SlackCommunication.h"
#include "TimeUtils.h"
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

std::string getError(const rapidjson::Document& jsonDoc)
{
    auto errorIt = jsonDoc.FindMember("error");
    if (errorIt == jsonDoc.MemberEnd()) {
        std::cerr << "Error, JSON does not contain 'error' key.\n";
        return std::string();
    }
    if (!errorIt->value.IsString()) {
        std::cerr << "Error, JSON 'error' is not string.\n";
        return std::string();
    }
    return errorIt->value.GetString();
}

template<typename Container>
std::string joinName(const Container &container, const std::string &separator = ",")
{
    std::string result = container.empty() ? std::string() : container.front().name;
    for (uint32_t i = 1; i < container.size(); ++i) {
        result += separator;
        result += container[i].name;
    }
    return result;
}

std::string joinString(const std::vector<std::string> &stringList, const std::string &separator = ",")
{
    std::string result = stringList.empty() ? std::string() : stringList[0];
    for (uint32_t i = 1; i < stringList.size(); ++i) {
        result += separator;
        result += stringList[i];
    }
    return result;
}

template<typename T>
void printJson(T& obj)
{
    rapidjson::StringBuffer buffer;
    rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(buffer);
    rapidjson::Value val(obj);
    val.Accept(writer);
    std::cout << buffer.GetString() << "\n";
    obj = val.GetObject(); // it is because how rapidjson has been designed
}

} // namespace

SlackCommunication::SlackCommunication(const Config &cfg)
    : m_cfg(cfg)
    , m_address(cfg.getValue("slackAddress"))
    , m_bearerId(cfg.getValue("slackBearerId"))
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

    m_urlEncodedHeaders.push_back(URL_ENC_HEADER);
    m_urlEncodedHeaders.push_back(authorizationHeader);

    m_multipartHeaders.push_back(MULTIPART_FORM_DATA_HEADER);
    m_multipartHeaders.push_back(authorizationHeader);

    //m_joinChannelAdr = m_address + "channels.join";
    m_chatPostAdr = m_address + "chat.postMessage";
    m_chatDeleteAdr = m_address + "chat.delete";
    m_fileUploadAdr = m_address + "files.upload";
    m_conversationHistoryAdr = m_address + "conversations.history";
    m_conversationListAdr = m_address + "conversations.list";
    m_conversationJoinAdr = m_address + "conversations.join";
}

SlackCommunication::~SlackCommunication()
{

}

SlackCommunication::JoinChannelAnswer SlackCommunication::joinChannel(const std::string& channelName, std::string* channelId)
{
    std::string takenChannelId;
    auto it = m_channelsNameToId.find(channelName);
    if (it == m_channelsNameToId.end()) {
        std::vector<Channel> channels; // there is no need to do this - I hope slack will no change their api again
        listConversations(channels);   // this is hack from my side because I don't want to wrap more api
        it = m_channelsNameToId.find(channelName);
        if (it == m_channelsNameToId.end()) {
            std::cerr << "Can't find channelId for: " << channelName;
            return JoinChannelAnswer::FAIL;
        }
    }
    takenChannelId = it->second;
    if (channelId) {
        *channelId = takenChannelId;
    }
    return joinConversation(takenChannelId);
}

/*
 * Unfortunatelly slack assumed it depricated and stoped to handle it.
 *
SlackCommunication::JoinChannelAnswer SlackCommunication::joinChannel(const std::string &channelName, std::string* channelId)
{
    if (m_alreadyJoinedChannels.find(channelName) != m_alreadyJoinedChannels.end()) {
        if (channelId) {
            auto it = m_channelsNameToId.find(channelName);
            if (it != m_channelsNameToId.end())
            {
                *channelId = it->second;
            }
        }
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

    if (channelId) {
        auto channelIt = jsonDoc.FindMember("channel");
        if (channelIt == jsonDoc.MemberEnd()) {
            std::cerr << "Error, JSON does not contain 'channel' key.\n";
        }
        else {
            auto channelObj = channelIt->value.GetObject();
            auto channelIdIt = channelObj.FindMember("id");
            if (channelIdIt == channelObj.MemberEnd()) {
                std::cerr << "Error, JSON does not contain 'channel' -> 'id' key.\n";
            }
            else {
                *channelId = channelIdIt->value.GetString();
                m_channelsNameToId[channelName] = *channelId;
            }

        }
    }

    if (isTrue("already_in_channel", jsonDoc)) {
        return JoinChannelAnswer::ALREADY_JOINED;
    }

    return JoinChannelAnswer::SUCCESS;
}
*/

SlackCommunication::GeneralAnswer SlackCommunication::listConversations(SlackCommunication::Channels &channels)
{
    auto respond = m_http.get(m_conversationListAdr, m_urlEncodedHeaders);
    if (respond.errorMsg.has_value()) {
        return GeneralAnswer::FAIL;
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
        return GeneralAnswer::FAIL;
    }

    if (!isTrue("ok", jsonDoc)) {
        std::cerr << "Server respond is: " << serverRespond << "\n";
        return GeneralAnswer::FAIL;
    }

    //
    // Parsing...
    //

    //std::cout << "Server respond is: " << serverRespond << "\n";
    auto jsonChannels = jsonDoc.FindMember("channels");
    if (jsonChannels == jsonDoc.MemberEnd()) {
        std::cerr << "Server respond is correct but does not contains channels: " << serverRespond << "\n";
        return GeneralAnswer::SUCCESS;
    }
    if (!jsonChannels->value.IsArray()) {
        std::cerr << "Server respond is correct but channels attribute is not an array: " << serverRespond << "\n";
        return GeneralAnswer::SUCCESS;
    }

    //
    // Taking channels...
    //

    auto rawChannels = jsonChannels->value.GetArray();

    for(rapidjson::SizeType i = 0; i < rawChannels.Size(); ++i) {
        if (!rawChannels[i].IsObject())
            continue;
        auto rawChannel = rawChannels[i].GetObject();
        //std::cout << "Raw channel: " << i << "\n";
        //printJson(rawChannel);
        //std::cout << "\n";

        auto idIt = rawChannel.FindMember("id");
        if (idIt == rawChannel.MemberEnd()){
            std::cerr << "channel object does not contains 'id'!\n";
            continue;
        }
        auto nameIt = rawChannel.FindMember("name");
        if (nameIt == rawChannel.MemberEnd()){
            std::cerr << "channel object does not contains 'name'!\n";
            continue;
        }
        Channel channel;
        channel.id = idIt->value.GetString();
        channel.name = nameIt->value.GetString();
        m_channelsNameToId[channel.name] = channel.id;
        channels.push_back(std::move(channel));
    }

    return GeneralAnswer::SUCCESS;
}

SlackCommunication::JoinChannelAnswer SlackCommunication::joinConversation(const std::string& channelId)
{
    if (m_alreadyJoinedChannels.find(channelId) != m_alreadyJoinedChannels.end()) {
        return JoinChannelAnswer::ALREADY_JOINED;
    }

    rapidjson::StringBuffer sb;
    rapidjson::Writer<rapidjson::StringBuffer> jsonWriter(sb);
    jsonWriter.StartObject();

    static const std::string kChannel { u8"channel" };
    jsonWriter.Key(kChannel.c_str(), static_cast<rapidjson::SizeType>(kChannel.length()));
    jsonWriter.String(channelId.c_str(), static_cast<rapidjson::SizeType>(channelId.length()));

    jsonWriter.EndObject();

    auto respond = m_http.post(m_conversationJoinAdr, m_jsonHeaders, sb.GetString());
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
    m_alreadyJoinedChannels.insert(channelId);

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

    Channel channel;
    channel.name = channelName;
    if (joinChannel(channelName, &channel.id) != SlackCommunication::JoinChannelAnswer::FAIL) {
        if (sendMessage(channelName, welcomeMsg) == SlackCommunication::SendAnswer::FAIL) {
            std::cerr << "Can't send welcome message\n";
            return false;
        }
        std::string botId;
        if (getUserBotId(botId) != GeneralAnswer::SUCCESS) {
            std::cerr << "Can't get bot id!\n";
        }
        else {
            std::cout << "Bot Id is: " << botId << "\n";
            std::cout.flush();
        }
        return true;
    }
    std::cerr << "Can't join Slack channel: " << channelName << "\n";
    return false;
}

SlackCommunication::SendAnswer SlackCommunication::sendFile(const Channels &channels, const std::string &filepath, SlackFileType fileType)
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

    std::string channelsStr = channels.empty() ? "general" : joinName(channels);

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
    channelsMime.data = HttpCommunication::BufferData{ channelsStr.data(), channelsStr.size() };
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


SlackCommunication::SendAnswer SlackCommunication::sendFile(const SlackCommunication::Channels &channels, const std::vector<char> &data, const std::string &filename, SlackFileType fileType)
{
    return sendFile(channels, data.data(), data.size(), filename, fileType);
}

SlackCommunication::SendAnswer SlackCommunication::sendFile(const SlackCommunication::Channels &channels, const char *data, size_t size, const std::string &filename, SlackFileType fileType)
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

    std::string channelsStr = channels.empty() ? "general" : joinName(channels);

    HttpCommunication::Mimes mimes;
    HttpCommunication::Mime file;
    file.name = "file";
    file.data = HttpCommunication::BufferData{ data, size };
    file.fileName = filename;
    file.fileType = slackFileTypeStr(fileType);
    mimes.push_back(std::move(file));

    HttpCommunication::Mime channelsMime;
    channelsMime.name = "channels";
    channelsMime.data =  HttpCommunication::BufferData{ channelsStr.data(), channelsStr.size() };
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

SlackCommunication::GeneralAnswer SlackCommunication::deleteMessageChannel(const std::string &channelId, const std::string &timeStamp)
{
    rapidjson::StringBuffer sb;
    rapidjson::Writer<rapidjson::StringBuffer> jsonWriter(sb);
    jsonWriter.StartObject();

    static const std::string kChannel { u8"channel" };
    jsonWriter.Key(kChannel.c_str(), static_cast<rapidjson::SizeType>(kChannel.length()));
    jsonWriter.String(channelId.c_str(), static_cast<rapidjson::SizeType>(channelId.length()));

    static const std::string kTimeStamp { u8"ts" };
    jsonWriter.Key(kTimeStamp.c_str(), static_cast<rapidjson::SizeType>(kTimeStamp.length()));
    jsonWriter.String(timeStamp.c_str(), static_cast<rapidjson::SizeType>(timeStamp.length()));

    //static const std::string kAsUser { u8"as_user" };
    //jsonWriter.Key(kAsUser.c_str(), static_cast<rapidjson::SizeType>(kAsUser.length()));
    //jsonWriter.Bool(true);

    jsonWriter.EndObject();


    auto respond = m_http.post(m_chatDeleteAdr, m_jsonHeaders, sb.GetString());
    if (respond.errorMsg.has_value()) {
        return GeneralAnswer::FAIL;
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
        return GeneralAnswer::FAIL;
    }
    if (!isTrue("ok", jsonDoc)) {
        std::string error = getError(jsonDoc);
        if (error == "ratelimited") {
            return GeneralAnswer::RATE_LIMITED;
        }
        std::cerr << "Server respond is: " << serverRespond << "\n";

        return GeneralAnswer::FAIL;
    }

    return GeneralAnswer::SUCCESS;
}

SlackCommunication::GeneralAnswer SlackCommunication::listChannelMessage(std::vector<Message>& messages,
                                                                         const std::string& channelId,
                                                                         const std::optional<uint32_t>& limit,
                                                                         const std::optional<std::string>& oldest,
                                                                         const std::optional<std::string>& latest,
                                                                         const std::optional<uint32_t>& inclusive)
{
    std::string data;
    static const std::string kChannel = m_http.urlEncode(u8"channel");
    static const std::string kLimit = m_http.urlEncode(u8"limit");
    static const std::string kOldest = m_http.urlEncode(u8"oldest");
    static const std::string kLatest = m_http.urlEncode(u8"latest");
    static const std::string kInclusive = m_http.urlEncode(u8"inclusive");
    data += kChannel;
    data += '=';
    data += m_http.urlEncode(channelId);

    if (limit.has_value()) {
        data += '&';
        data += kLimit;
        data += '=';
        data += m_http.urlEncode(std::to_string(limit.value()));
    }
    if (oldest.has_value()) {
        data += '&';
        data += kOldest;
        data += '=';
        data += m_http.urlEncode(oldest.value());
    }
    if (latest.has_value()) {
        data += '&';
        data += kLatest;
        data += '=';
        data += m_http.urlEncode(latest.value());
    }
    if (inclusive.has_value()) {
        data += '&';
        data += kInclusive;
        data += '=';
        data += m_http.urlEncode(std::to_string(inclusive.value()));
    }

    //
    // Sending...
    //

    auto respond = m_http.post(m_conversationHistoryAdr, m_urlEncodedHeaders, data);
    if (respond.errorMsg.has_value()) {
        return GeneralAnswer::FAIL;
    }
    assert(respond.serverAnswer.has_value());
    std::string& serverRespond = respond.serverAnswer.value();

    //
    // Result...
    //

    rapidjson::Document jsonDoc;
    jsonDoc.Parse(serverRespond.c_str(), static_cast<rapidjson::SizeType>(serverRespond.length()));
    if (jsonDoc.HasParseError()) {
        rapidjson::ParseErrorCode error = jsonDoc.GetParseError();
        std::cerr << "Error during parsing JSON: " << error << " (" << rapidjson::GetParseError_En(error) << ")\n";
        std::cerr << "Server respond is: " << serverRespond << "\n";
        return GeneralAnswer::FAIL;
    }
    assert(jsonDoc.IsObject());
    if (!isTrue("ok", jsonDoc)) {
        std::string error = getError(jsonDoc);
        if (error == "ratelimited") {
            return GeneralAnswer::RATE_LIMITED;
        }
        std::cerr << "Server respond is: " << serverRespond << "\n";
        return GeneralAnswer::FAIL;
    }

    //
    // Parsing...
    //

    //std::cout << "Server respond is: " << serverRespond << "\n";
    auto jsonMessages = jsonDoc.FindMember("messages");
    if (jsonMessages == jsonDoc.MemberEnd()) {
        std::cerr << "Server respond is correct but does not contains messages: " << serverRespond << "\n";
        return GeneralAnswer::SUCCESS;
    }
    if (!jsonMessages->value.IsArray()) {
        std::cerr << "Server respond is correct but messages attribute is not an array: " << serverRespond << "\n";
        return GeneralAnswer::SUCCESS;
    }

    //
    // Taking messages...
    //

    auto rawMessages = jsonMessages->value.GetArray();

    for(rapidjson::SizeType i = 0; i < rawMessages.Size(); ++i) {
        if (!rawMessages[i].IsObject())
            continue;
        auto rawMessage = rawMessages[i].GetObject();
        //std::cout << "Raw message: " << i << "\n";
        //printJson(rawMessage);
        //std::cout << "\n";
        //std::cout.flush();

        auto typeIt = rawMessage.FindMember("type");
        if (typeIt == rawMessage.MemberEnd()){
            std::cerr << "message object does not contains 'type'!\n";
            continue;
        }
        auto timeStampIt = rawMessage.FindMember("ts");
        if (timeStampIt == rawMessage.MemberEnd()){
            std::cerr << "message object does not contains 'ts'!\n";
            continue;
        }
        Message msg;
        msg.type = typeIt->value.GetString();
        msg.timeStamp = timeStampIt->value.GetString();

        #define takeJsonValue(name, variable, method) \
            {\
                auto it = rawMessage.FindMember(name);\
                if (it != rawMessage.MemberEnd()) {\
                    msg.variable = it->value.method();\
                }\
            }

        takeJsonValue("bot_id", botId, GetString);
        takeJsonValue("text", text, GetString);
        takeJsonValue("user", user, GetString);
        takeJsonValue("team", team, GetString);

        #undef takeJsonValue

        messages.push_back(std::move(msg));
    }

    return GeneralAnswer::SUCCESS;
}

SlackCommunication::GeneralAnswer SlackCommunication::lastChannelMessage(const std::string &channelId, SlackCommunication::Message &message)
{
    std::vector<Message> messages;
    auto result = listChannelMessage(messages, channelId, 1);
    if (!messages.empty()) {
        message = messages.front();
    }
    return result;
}

SlackCommunication::GeneralAnswer SlackCommunication::getUserBotId(std::string& id)
{
    if (!m_botId.empty()) {
        id = m_botId;
        return GeneralAnswer::SUCCESS;
    }

    // There is no easy way to get bot id - 'like who I am'
    // https://api.slack.com/methods/bots.info returns me nothing when I provide only token :(
    // I haven't checked https://api.slack.com/methods/users.list
    // So for me the easiest way is to send some message and then list it back with additional info

    Channel channel;
    channel.name = m_cfg.getValue("slackReportChannel", "general");
    if (auto result = joinChannel(channel.name, &channel.id); result == JoinChannelAnswer::FAIL) {
        std::cerr << "Can't join channel.\n";
        return GeneralAnswer::FAIL;
    }

    std::string uniqueText = TimeUtils::currentTimeStamp() + "_find_bot";
    if (auto result = sendMessage(channel.name, uniqueText); result != SendAnswer::SUCCESS) {
        std::cerr << "Can't send message. Result: " << static_cast<int>(result) << "\n";
        return GeneralAnswer::FAIL;
    }
    std::vector<Message> messages;
    auto oldestTs = TimeUtils::timePointToTimeStamp(std::chrono::system_clock::now() - std::chrono::seconds(3));
    if (auto result = listChannelMessage(messages, channel.id, 10, oldestTs); result != GeneralAnswer::SUCCESS) {
        std::cerr << "Can't get messages. Result: " << static_cast<int>(result) << "\n";
        return GeneralAnswer::FAIL;
    }
    for (const auto& message : messages) {
        if (message.text.has_value() && message.text.value() == uniqueText) {
            deleteMessageChannel(channel.id, message.timeStamp);
            m_botId = message.user.has_value() ? message.user.value() : "";
            id = m_botId;
            return GeneralAnswer::SUCCESS;
        }
    }
    std::cerr << "Message just sent and can't be found in lates messages - it shouldn't be that in this case.";
    return GeneralAnswer::FAIL;
}

std::string SlackCommunication::msgToStr(const Message& msg)
{
    std::string result;
    result += "Message type: " + msg.type + "\n";
    result += "Message time: " + msg.timeStamp + "\n";
    result += "Message text: " + (msg.text.has_value() ? msg.text.value() : "there is no text info") + "\n";
    result += "Message user: " + (msg.user.has_value() ? msg.user.value() : "there is no user info") + "\n";
    result += "Message bot : " + (msg.botId.has_value() ? msg.botId.value() : "there is no bot info") + "\n";
    result += "Message team: " + (msg.team.has_value() ? msg.team.value() : "there is no team info") + "\n";
    return result;
}
