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

    Response get(const std::string& address, const HeaderList &headers, const std::string& data = std::string());
    Response post(const std::string &address, const HeaderList &headers, const std::string &data, const Mimes &mimes = Mimes());
    Response put();

    std::string urlEncode(const std::string &str);
    std::string urlDecode(const std::string &str);

private:
    enum class MSG_TYPE {
        GET, POST, PUT
    };
    Response setMsgType(MSG_TYPE type);
    Response executeRequest();

    void *m_easyhandle = nullptr;
};
