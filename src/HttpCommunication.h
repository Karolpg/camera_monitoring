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

    struct BufferData {
        const char* data = nullptr;             // some data if not provide any filePath
        size_t size = 0;
    };

    struct Mime {
        std::string name;
        std::optional<std::string> filepath;   // local path to file which has to be uploaded
        std::optional<BufferData>  data;       // some data if not provide any filePath
        std::optional<std::string> fileName;   // can be anything e.g. "abc.jpg"
        std::optional<std::string> fileType;   // e.g. "jpg"
        std::optional<std::string> encoder;    // e.g. "base64"
        //data_cb
    };
    using Mimes = std::vector<Mime>;

    HttpCommunication();
    ~HttpCommunication();

    Response get();
    Response post(const std::string &address, const HeaderList &headers, const std::string &data, const Mimes &mimes = Mimes());
    Response put();

private:
    enum class MSG_TYPE {
        GET, POST, PUT
    };
    Response setMsgType(MSG_TYPE type);
    Response executeRequest();

    void *m_easyhandle = nullptr;
};
