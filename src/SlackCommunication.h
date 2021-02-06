#pragma once

#include <string>
#include <set>
#include <map>
#include <optional>
#include "HttpCommunication.h"
#include "SlackFileTypes.h"

class SlackCommunication
{
public:
    SlackCommunication(const std::string& address, const std::string& bearerId);
    ~SlackCommunication();

    using Channels = std::vector<std::string>;

    enum class GeneralAnswer
    {
        SUCCESS,
        FAIL,
        RATE_LIMITED
    };

    enum class JoinChannelAnswer
    {
        SUCCESS,
        ALREADY_JOINED,
        FAIL
    };
    struct Channel {
        std::string id;
        std::string name;
    };

    JoinChannelAnswer joinChannel(const std::string& channelName, std::string* channelId = nullptr);
    GeneralAnswer listConversations(std::vector<Channel>& channels);
    JoinChannelAnswer joinConversation(const std::string& channelId);


    enum class SendAnswer
    {
        SUCCESS,
        CHANNEL_NOT_EXIST,
        FAIL
    };

    struct Message {
        std::string type;
        std::string timeStamp;

        std::optional<std::string> text;
        std::optional<std::string> user;
        std::optional<std::string> botId;
        std::optional<std::string> team;
    };


    SendAnswer sendMessage(const std::string& channelName, const std::string& text);
    bool sendWelcomMessage(const std::string& channelName);
    SendAnswer sendFile(const Channels& channelNames, const std::string& filepath, SlackFileType fileType = SlackFileType::autoType);
    SendAnswer sendFile(const Channels& channelNames, const char* data, size_t size, const std::string& filename, SlackFileType fileType);
    SendAnswer sendFile(const Channels& channelNames, const std::vector<char>& data, const std::string& filename, SlackFileType fileType);

    GeneralAnswer deleteMessageChannel(const std::string& channelName, const std::string& timeStamp);

    GeneralAnswer listChannelMessage(std::vector<Message>& messages,
                                     const std::string& channelId,
                                     const std::optional<uint32_t>& limit = std::optional<uint32_t>(),         // default 100 - The maximum number of items to return.
                                                                                                               //               Fewer than the requested number of items may be returned,
                                                                                                               //               even if the end of the users list hasn't been reached.
                                     const std::optional<std::string>& oldest = std::optional<std::string>(),  // default 0   - Start of time range of messages to include in results.
                                     const std::optional<std::string>& latest = std::optional<std::string>(),  // default now - End of time range of messages to include in results.
                                     const std::optional<uint32_t>& inclusive = std::optional<uint32_t> ()     // default 0   - Include messages with latest or oldest timestamp in results
                                                                                                               //               only when either timestamp is specified.
                                     );


    GeneralAnswer lastChannelMessage(const std::string& channelId, Message& message);

private:
    std::string m_address;
    std::string m_bearerId;
    HttpCommunication m_http;

    HttpCommunication::HeaderList m_jsonHeaders;
    HttpCommunication::HeaderList m_urlEncodedHeaders;
    HttpCommunication::HeaderList m_multipartHeaders;
    //std::string m_joinChannelAdr;
    std::string m_chatPostAdr;
    std::string m_chatDeleteAdr;
    std::string m_fileUploadAdr;
    std::string m_conversationHistoryAdr;
    std::string m_conversationListAdr;
    std::string m_conversationJoinAdr;

    std::set<std::string> m_alreadyJoinedChannels;
    std::map<std::string, std::string> m_channelsNameToId;
};
