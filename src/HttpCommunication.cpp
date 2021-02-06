#include "HttpCommunication.h"

#include <curl/curl.h>
#include <iostream>

namespace  {

class CurlList {
public:
    ~CurlList() { curl_slist_free_all(m_list); }

    void add(const std::string& header) { m_list = curl_slist_append(m_list, header.c_str()); }

    curl_slist *listObj() const { return m_list; }

private:
    curl_slist *m_list = nullptr;

};

class MimeWrapper {
public:
    MimeWrapper(CURL *curlhandle, const HttpCommunication::Mimes &mimes) {
        if (mimes.empty())
            return;
        m_multipart = curl_mime_init(curlhandle);
        for (const auto& mime : mimes) {
            curl_mimepart *part = curl_mime_addpart(m_multipart);
            curl_mime_name(part, mime.name.c_str());
            if (mime.filepath.has_value()) curl_mime_filedata(part, mime.filepath.value().c_str());
            if (mime.fileName.has_value()) curl_mime_filename(part, mime.fileName.value().c_str());
            if (mime.fileType.has_value()) curl_mime_type    (part, mime.fileType.value().c_str());
            if (mime.encoder.has_value() ) curl_mime_encoder (part, mime.encoder.value().c_str() );

            if (mime.data.has_value())     curl_mime_data    (part, mime.data.value().data, mime.data.value().size);
            //if (mime.data_cb.has_value()) curl_mime_data_cb(part, (curl_off_t) datasize, myreadfunc, NULL, NULL, arg);  // TODO :)
        }
    }
    ~MimeWrapper() {
        if (m_multipart) {
           curl_mime_free(m_multipart);
        }
    }

    curl_mime *mimeObj() const { return m_multipart; }
private:
    curl_mime *m_multipart = nullptr;
};

static const char* curlCodeToStr(CURLcode code)
{
    switch(code) {
    case CURLE_OK                        : return "CURLE_OK";
    case CURLE_UNSUPPORTED_PROTOCOL      : return "CURLE_UNSUPPORTED_PROTOCOL";
    case CURLE_FAILED_INIT               : return "CURLE_FAILED_INIT";
    case CURLE_URL_MALFORMAT             : return "CURLE_URL_MALFORMAT";
    case CURLE_NOT_BUILT_IN              : return "CURLE_NOT_BUILT_IN - was obsoleted in August 2007 for 7.17.0, reused in April 2011 for 7.21.5]";
    case CURLE_COULDNT_RESOLVE_PROXY     : return "CURLE_COULDNT_RESOLVE_PROXY";
    case CURLE_COULDNT_RESOLVE_HOST      : return "CURLE_COULDNT_RESOLVE_HOST";
    case CURLE_COULDNT_CONNECT           : return "CURLE_COULDNT_CONNECT";
    case CURLE_WEIRD_SERVER_REPLY        : return "CURLE_WEIRD_SERVER_REPLY";
    case CURLE_REMOTE_ACCESS_DENIED      : return "CURLE_REMOTE_ACCESS_DENIED - A service was denied by the server due to lack of access - "
                                                  "when login fails this is not returned.";
    case CURLE_FTP_ACCEPT_FAILED         : return "CURLE_FTP_ACCEPT_FAILED - [was obsoleted in April 2006 for 7.15.4, reused in Dec 2011 for 7.24.0]";
    case CURLE_FTP_WEIRD_PASS_REPLY      : return "CURLE_FTP_WEIRD_PASS_REPLY";
    case CURLE_FTP_ACCEPT_TIMEOUT        : return "CURLE_FTP_ACCEPT_TIMEOUT - Timeout occurred accepting server "
                                                  "[was obsoleted in August 2007 for 7.17.0, reused in Dec 2011 for 7.24.0]";
    case CURLE_FTP_WEIRD_PASV_REPLY      : return "CURLE_FTP_WEIRD_PASV_REPLY";
    case CURLE_FTP_WEIRD_227_FORMAT      : return "CURLE_FTP_WEIRD_227_FORMAT";
    case CURLE_FTP_CANT_GET_HOST         : return "CURLE_FTP_CANT_GET_HOST";
    case CURLE_HTTP2                     : return "CURLE_HTTP2 - A problem in the http2 framing layer. "
                                                  "[was obsoleted in August 2007 for 7.17.0, reused in July 2014 for 7.38.0]";
    case CURLE_FTP_COULDNT_SET_TYPE      : return "CURLE_FTP_COULDNT_SET_TYPE";
    case CURLE_PARTIAL_FILE              : return "CURLE_PARTIAL_FILE";
    case CURLE_FTP_COULDNT_RETR_FILE     : return "CURLE_FTP_COULDNT_RETR_FILE";
    case CURLE_OBSOLETE20                : return "CURLE_OBSOLETE20 NOT USED";
    case CURLE_QUOTE_ERROR               : return "CURLE_QUOTE_ERROR quote command failure";
    case CURLE_HTTP_RETURNED_ERROR       : return "CURLE_HTTP_RETURNED_ERROR";
    case CURLE_WRITE_ERROR               : return "CURLE_WRITE_ERROR";
    case CURLE_OBSOLETE24                : return "CURLE_OBSOLETE24 NOT USED";
    case CURLE_UPLOAD_FAILED             : return "CURLE_UPLOAD_FAILED - failed upload \"command\"";
    case CURLE_READ_ERROR                : return "CURLE_READ_ERROR - couldn't open/read from file";
    case CURLE_OUT_OF_MEMORY             : return "CURLE_OUT_OF_MEMORY - may sometimes indicate a conversion error "
                                                  "instead of a memory allocation error if CURL_DOES_CONVERSIONS is defined";
    case CURLE_OPERATION_TIMEDOUT        : return "CURLE_OPERATION_TIMEDOUT the timeout time was reached";
    case CURLE_OBSOLETE29                : return "CURLE_OBSOLETE29 NOT USED";
    case CURLE_FTP_PORT_FAILED           : return "CURLE_FTP_PORT_FAILED - FTP PORT operation failed";
    case CURLE_FTP_COULDNT_USE_REST      : return "CURLE_FTP_COULDNT_USE_REST - the REST command failed";
    case CURLE_OBSOLETE32                : return "CURLE_OBSOLETE32 - NOT USED";
    case CURLE_RANGE_ERROR               : return "CURLE_RANGE_ERROR - RANGE \"command\" didn't work";
    case CURLE_HTTP_POST_ERROR           : return "CURLE_HTTP_POST_ERROR";
    case CURLE_SSL_CONNECT_ERROR         : return "CURLE_SSL_CONNECT_ERROR - wrong when connecting with SSL";
    case CURLE_BAD_DOWNLOAD_RESUME       : return "CURLE_BAD_DOWNLOAD_RESUME - couldn't resume download";
    case CURLE_FILE_COULDNT_READ_FILE    : return "CURLE_FILE_COULDNT_READ_FILE";
    case CURLE_LDAP_CANNOT_BIND          : return "CURLE_LDAP_CANNOT_BIND";
    case CURLE_LDAP_SEARCH_FAILED        : return "CURLE_LDAP_SEARCH_FAILED";
    case CURLE_OBSOLETE40                : return "CURLE_OBSOLETE40 NOT USED";
    case CURLE_FUNCTION_NOT_FOUND        : return "CURLE_FUNCTION_NOT_FOUND - NOT USED starting with 7.53.0";
    case CURLE_ABORTED_BY_CALLBACK       : return "CURLE_ABORTED_BY_CALLBACK";
    case CURLE_BAD_FUNCTION_ARGUMENT     : return "CURLE_BAD_FUNCTION_ARGUMENT";
    case CURLE_OBSOLETE44                : return "CURLE_OBSOLETE44 NOT USED";
    case CURLE_INTERFACE_FAILED          : return "CURLE_INTERFACE_FAILED - CURLOPT_INTERFACE failed";
    case CURLE_OBSOLETE46                : return "CURLE_OBSOLETE46 - NOT USED";
    case CURLE_TOO_MANY_REDIRECTS        : return "CURLE_TOO_MANY_REDIRECTS - catch endless re-direct loops";
    case CURLE_UNKNOWN_OPTION            : return "CURLE_UNKNOWN_OPTION - User specified an unknown option";
    case CURLE_TELNET_OPTION_SYNTAX      : return "CURLE_TELNET_OPTION_SYNTAX - Malformed telnet option";
    case CURLE_OBSOLETE50                : return "CURLE_OBSOLETE50 - NOT USED";
    case CURLE_PEER_FAILED_VERIFICATION  : return "CURLE_PEER_FAILED_VERIFICATION - peer's certificate or "
                                                  "fingerprint wasn't verified fine";
    case CURLE_GOT_NOTHING               : return "CURLE_GOT_NOTHING - when this is a specific error";
    case CURLE_SSL_ENGINE_NOTFOUND       : return "CURLE_SSL_ENGINE_NOTFOUND - SSL crypto engine not found";
    case CURLE_SSL_ENGINE_SETFAILED      : return "CURLE_SSL_ENGINE_SETFAILED - can not set SSL crypto engine as default";
    case CURLE_SEND_ERROR                : return "CURLE_SEND_ERROR - failed sending network data";
    case CURLE_RECV_ERROR                : return "CURLE_RECV_ERROR - failure in receiving network data";
    case CURLE_OBSOLETE57                : return "CURLE_OBSOLETE57 - NOT IN USE";
    case CURLE_SSL_CERTPROBLEM           : return "CURLE_SSL_CERTPROBLEM - problem with the local certificate";
    case CURLE_SSL_CIPHER                : return "CURLE_SSL_CIPHER - couldn't use specified cipher";
    case CURLE_SSL_CACERT                : return "CURLE_SSL_CACERT - problem with the CA cert (path?)";
    case CURLE_BAD_CONTENT_ENCODING      : return "CURLE_BAD_CONTENT_ENCODING - Unrecognized/bad encoding";
    case CURLE_LDAP_INVALID_URL          : return "CURLE_LDAP_INVALID_URL - Invalid LDAP URL";
    case CURLE_FILESIZE_EXCEEDED         : return "CURLE_FILESIZE_EXCEEDED - Maximum file size exceeded";
    case CURLE_USE_SSL_FAILED            : return "CURLE_USE_SSL_FAILED - Requested FTP SSL level failed";
    case CURLE_SEND_FAIL_REWIND          : return "CURLE_SEND_FAIL_REWIND - Sending the data requires a rewind that failed";
    case CURLE_SSL_ENGINE_INITFAILED     : return "CURLE_SSL_ENGINE_INITFAILED - failed to initialise ENGINE";
    case CURLE_LOGIN_DENIED              : return "CURLE_LOGIN_DENIED - user, password or similar was not accepted and we failed to login";
    case CURLE_TFTP_NOTFOUND             : return "CURLE_TFTP_NOTFOUND - file not found on server";
    case CURLE_TFTP_PERM                 : return "CURLE_TFTP_PERM - permission problem on server";
    case CURLE_REMOTE_DISK_FULL          : return "CURLE_REMOTE_DISK_FULL - out of disk space on server";
    case CURLE_TFTP_ILLEGAL              : return "CURLE_TFTP_ILLEGAL - Illegal TFTP operation";
    case CURLE_TFTP_UNKNOWNID            : return "CURLE_TFTP_UNKNOWNID - Unknown transfer ID";
    case CURLE_REMOTE_FILE_EXISTS        : return "CURLE_REMOTE_FILE_EXISTS - File already exists";
    case CURLE_TFTP_NOSUCHUSER           : return "CURLE_TFTP_NOSUCHUSER - No such user";
    case CURLE_CONV_FAILED               : return "CURLE_CONV_FAILED - conversion failed";
    case CURLE_CONV_REQD                 : return "CURLE_CONV_REQD - caller must register conversion callbacks using curl_easy_setopt options CURLOPT_CONV_FROM_NETWORK_FUNCTION, CURLOPT_CONV_TO_NETWORK_FUNCTION, CURLOPT_CONV_FROM_UTF8_FUNCTION";
    case CURLE_SSL_CACERT_BADFILE        : return "CURLE_SSL_CACERT_BADFILE - could not load CACERT file, missing or wrong format";
    case CURLE_REMOTE_FILE_NOT_FOUND     : return "CURLE_REMOTE_FILE_NOT_FOUND - remote file not found";
    case CURLE_SSH                       : return "CURLE_SSH - error from the SSH layer, somewhat generic so the error message will be of interest when this has happened";
    case CURLE_SSL_SHUTDOWN_FAILED       : return "CURLE_SSL_SHUTDOWN_FAILED - Failed to shut down the SSL connection";
    case CURLE_AGAIN                     : return "CURLE_AGAIN - socket is not ready for send/recv, wait till it's ready and try again (Added in 7.18.2)";
    case CURLE_SSL_CRL_BADFILE           : return "CURLE_SSL_CRL_BADFILE - could not load CRL file, missing or wrong format (Added in 7.19.0)";
    case CURLE_SSL_ISSUER_ERROR          : return "CURLE_SSL_ISSUER_ERROR - Issuer check failed.  (Added in 7.19.0)";
    case CURLE_FTP_PRET_FAILED           : return "CURLE_FTP_PRET_FAILED - a PRET command failed";
    case CURLE_RTSP_CSEQ_ERROR           : return "CURLE_RTSP_CSEQ_ERROR - mismatch of RTSP CSeq numbers";
    case CURLE_RTSP_SESSION_ERROR        : return "CURLE_RTSP_SESSION_ERROR - mismatch of RTSP Session Ids";
    case CURLE_FTP_BAD_FILE_LIST         : return "CURLE_FTP_BAD_FILE_LIST - unable to parse FTP file list";
    case CURLE_CHUNK_FAILED              : return "CURLE_CHUNK_FAILED - chunk callback reported error";
    case CURLE_NO_CONNECTION_AVAILABLE   : return "CURLE_NO_CONNECTION_AVAILABLE - No connection available, the session will be queued";
    case CURLE_SSL_PINNEDPUBKEYNOTMATCH  : return "CURLE_SSL_PINNEDPUBKEYNOTMATCH - specified pinned public key did not match";
    case CURLE_SSL_INVALIDCERTSTATUS     : return "CURLE_SSL_INVALIDCERTSTATUS - invalid certificate status";
    case CURLE_HTTP2_STREAM              : return "CURLE_HTTP2_STREAM - stream error in HTTP/2 framing layer";
    default                              : break;
    }
    return "Unknow Flag";
}

#define SET_OPT_HTTP_COM(opt, value) \
    if (CURLcode setoptResult = curl_easy_setopt(m_easyhandle, opt, value); setoptResult != CURLE_OK) { \
        std::cerr << "curl_easy_setopt(" << m_easyhandle <<  ", " #opt ", " << value << ") " \
                  << "Error: " << setoptResult << " " << curlCodeToStr(setoptResult) << "\n" << curl_easy_strerror(setoptResult) << "\n"; \
        return {{}, std::string("Error: ") + curlCodeToStr(setoptResult) + " " + curl_easy_strerror(setoptResult) }; \
    }

size_t writeStr(void *buffer, size_t size, size_t nmemb, std::string *str)
{
    std::string& strRef = *str;
    auto l = strRef.length();
    strRef.resize(l + size * nmemb, ' ');
    char* data = reinterpret_cast<char*>(buffer);
    std::copy(data, data + size * nmemb, strRef.data() + l);
    return size * nmemb;
}

} // namespace

HttpCommunication::HttpCommunication()
{
    long flags = CURL_GLOBAL_DEFAULT; //CURL_GLOBAL_ALL;
    CURLcode code = curl_global_init(flags);
    if (code != CURLE_OK) {
        std::cerr << "Can't init CURL global. Error: " << code << " " << curlCodeToStr(code) << "\n";
        return;
    }

    m_easyhandle = curl_easy_init();
    if (!m_easyhandle) {
        std::cerr << "Can't init easy CURL object\n";
    }
}

HttpCommunication::~HttpCommunication()
{
    if (m_easyhandle) {
        curl_easy_cleanup(m_easyhandle);
    }

    curl_global_cleanup();
}


HttpCommunication::Response HttpCommunication::get(const std::string& address,
                                                   const HeaderList &headers,
                                                   const std::string& data)
{
    //std::cout << "--------------------------------------------------------------------------GET\n";
    //std::cout.flush();

    if (!m_easyhandle) {
        static const std::string msg("Easy-handle wasn't initialized! Get can't be berformed!");
        std::cerr << msg << "\n";
        return {{}, msg};
    }

    if (auto response = setMsgType(MSG_TYPE::GET); response.errorMsg.has_value()) {
        return response;
    }

    SET_OPT_HTTP_COM(CURLOPT_URL, address.c_str());
    //SET_OPT_HTTP_COM(CURLOPT_NOBODY, 0L);


    CurlList headerObj;
    if (!headers.empty()) {
        for (const auto& h : headers) {
            headerObj.add(h.c_str());
        }
        SET_OPT_HTTP_COM(CURLOPT_HTTPHEADER, headerObj.listObj());
    }

    if (!data.empty()) {
        SET_OPT_HTTP_COM(CURLOPT_POSTFIELDSIZE, data.size());
        SET_OPT_HTTP_COM(CURLOPT_POSTFIELDS, data.data());
    }
    else {
        SET_OPT_HTTP_COM(CURLOPT_POSTFIELDSIZE, 0);
        SET_OPT_HTTP_COM(CURLOPT_POSTFIELDS, static_cast<const char*>(nullptr));
    }

    return executeRequest();
}

HttpCommunication::Response HttpCommunication::post(const std::string& address,
                                                    const HeaderList &headers,
                                                    const std::string& data,
                                                    const Mimes &mimes)
{
    //std::cout << "--------------------------------------------------------------------------POST\n";
    //std::cout.flush();

    if (!m_easyhandle) {
        static const std::string msg("Easy-handle wasn't initialized! Post can't be berformed!");
        std::cerr << msg << "\n";
        return {{}, msg};
    }

    if (auto response = setMsgType(MSG_TYPE::POST); response.errorMsg.has_value()) {
        return response;
    }

    SET_OPT_HTTP_COM(CURLOPT_URL, address.c_str());

    if (!data.empty()) {
        SET_OPT_HTTP_COM(CURLOPT_POSTFIELDSIZE, data.size());
        SET_OPT_HTTP_COM(CURLOPT_POSTFIELDS, data.data());
    }
    else {
        SET_OPT_HTTP_COM(CURLOPT_POSTFIELDSIZE, 0);
        SET_OPT_HTTP_COM(CURLOPT_POSTFIELDS, static_cast<const char*>(nullptr));
    }

    //SET_OPT_HTTP_COM(CURLOPT_VERBOSE, 1L);

    CurlList headerObj;
    if (!headers.empty()) {
        for (const auto& h : headers) {
            headerObj.add(h.c_str());
        }
        SET_OPT_HTTP_COM(CURLOPT_HTTPHEADER, headerObj.listObj());
    }
    else {
        SET_OPT_HTTP_COM(CURLOPT_HTTPHEADER, static_cast<curl_slist*>(nullptr));
    }

    MimeWrapper mimeWrapper(m_easyhandle, mimes);
    if (mimeWrapper.mimeObj()) {
        SET_OPT_HTTP_COM(CURLOPT_MIMEPOST, mimeWrapper.mimeObj());
    }

    return executeRequest();
}

HttpCommunication::Response HttpCommunication::put()
{
    //std::cout << "--------------------------------------------------------------------------PUT\n";
    //std::cout.flush();

    if (!m_easyhandle) {
        static const std::string msg("Easy-handle wasn't initialized! Post can't be berformed!");
        std::cerr << msg << "\n";
        return {{}, msg};
    }

    if (auto response = setMsgType(MSG_TYPE::PUT); response.errorMsg.has_value()) {
        return response;
    }

    // TODO add PUT data

    return executeRequest();
}

std::string HttpCommunication::urlEncode(const std::string &str)
{
    char *output = curl_easy_escape(m_easyhandle, str.c_str(), static_cast<int>(str.size()));
    std::string result = output;
    curl_free(output);
    return result;
}

std::string HttpCommunication::urlDecode(const std::string &str)
{
    int outLen = 0;
    char *output = curl_easy_unescape(m_easyhandle, str.c_str(), static_cast<int>(str.size()), &outLen);
    std::string result(output, static_cast<size_t>(outLen));
    curl_free(output);
    return result;
}

HttpCommunication::Response HttpCommunication::setMsgType(HttpCommunication::MSG_TYPE type)
{
    switch(type) {
    case MSG_TYPE::GET:
        SET_OPT_HTTP_COM(CURLOPT_HTTPGET, 1L);
        SET_OPT_HTTP_COM(CURLOPT_POST, 0L);
        SET_OPT_HTTP_COM(CURLOPT_PUT, 0L);
        break;
    case MSG_TYPE::POST:
        SET_OPT_HTTP_COM(CURLOPT_HTTPGET, 0L);
        SET_OPT_HTTP_COM(CURLOPT_POST, 1L);
        SET_OPT_HTTP_COM(CURLOPT_PUT, 0L);
        break;
    case MSG_TYPE::PUT:
        SET_OPT_HTTP_COM(CURLOPT_HTTPGET, 0L);
        SET_OPT_HTTP_COM(CURLOPT_POST, 0L);
        SET_OPT_HTTP_COM(CURLOPT_PUT, 1L);
        break;
    default:
        break;
    }
    return {{}, {}};
}

HttpCommunication::Response HttpCommunication::executeRequest()
{
    std::string responseDataString;
    std::string responseHeaderString;
    SET_OPT_HTTP_COM(CURLOPT_WRITEFUNCTION, writeStr);
    SET_OPT_HTTP_COM(CURLOPT_WRITEDATA, &responseDataString);
    SET_OPT_HTTP_COM(CURLOPT_HEADERDATA, &responseHeaderString);

    if (CURLcode performResult = curl_easy_perform(m_easyhandle); performResult != CURLE_OK) {
        std::cerr << "curl_easy_perform Error: " << performResult << " " << curlCodeToStr(performResult) << "\n" << curl_easy_strerror(performResult) << "\n";
        curl_easy_reset(m_easyhandle);
        return { {}, {std::string("Error: ") + curlCodeToStr(performResult) + " " + curl_easy_strerror(performResult)}};
    }
    curl_easy_reset(m_easyhandle);
    return {responseDataString, {}};
}


