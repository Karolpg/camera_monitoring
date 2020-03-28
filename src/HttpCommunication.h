#pragma once

#include <vector>
#include <string>
#include <optional>

class HttpCommunication
{
public:
    using HeaderList = std::vector<std::string>;
    struct Response {
        std::optional<std::string> serverAnswer;
        std::optional<std::string> errorMsg;
    };

    HttpCommunication();
    ~HttpCommunication();

    Response get();
    Response post(const std::string &address, const HeaderList &headers, const std::string &data);
    Response put();

private:
    enum class MSG_TYPE {
        GET, POST, PUT
    };
    Response setMsgType(MSG_TYPE type);
    Response executeRequest();

    void *m_easyhandle = nullptr;
};
