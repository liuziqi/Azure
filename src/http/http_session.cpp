#include "http/http_session.h"
#include "http/http_parser.h"
#include "log.h"
#include "macro.h"

namespace azure {

static azure::Logger::ptr g_logger = AZURE_LOG_ROOT();

namespace http {

HttpSession::HttpSession(Socket::ptr sock, bool owner)
    : SocketStream(sock, owner) {
}

HttpRequest::ptr HttpSession::recvRequest() {
    HttpRequestParser::ptr parser(new HttpRequestParser);
    uint64_t buffer_size = HttpRequestParser::GetHttpRequestBufferSize();
    // uint64_t buffer_size = 100;
    std::shared_ptr<char> buffer(new char[buffer_size], [](char *ptr){delete[] ptr;});
    char *data = buffer.get();
    // memset(data, '\0', buffer_size);    // DEBUG 不加会出错
    int offset = 0;

    do {
        int len = read(data + offset, buffer_size - offset);
        if(len <= 0) {
            // AZURE_LOG_INFO(g_logger) << " len=" << len;
            close();
            return nullptr;
        }
        len += offset;
        size_t nparse = parser->execute(data, len);
        if(parser->hasError()) {
            close();
            return nullptr;
        }
        offset = len - nparse;
        if(offset == (int)buffer_size) {
            close();
            return nullptr;
        }
        if(parser->isFinished()) {
            break;
        }
    } while(true);

    // size_t s = strlen(data);
    // AZURE_ASSERT2(data[s - 1] == '\n' && data[s - 2] == '\r', "request parser fail");
    // AZURE_LOG_INFO(g_logger) << "data:" << std::endl << data;

    int64_t length = parser->getContentLength();
    if(length > 0) {
        std::string body;
        body.resize(length);

        int len = 0;
        if(length >= offset) {
            memcpy(&body[0], data, offset);
            len = offset;
        }
        else {
            memcpy(&body[0], data, length);
            len = length;
        }
        length -= offset;
        if(length > 0) {
            if(readFixSize(&body[len], length) <= 0) {
                close();
                return nullptr;
            }
        }
        parser->getData()->setBody(body);
    }

    // DEBUG 这里可能因为大小写出问题
    std::string keep_alive = parser->getData()->getHeader("Connection");
    if(strcasecmp(keep_alive.c_str(), "keep-alive") == 0) {
        parser->getData()->setClose(false);
    }

    return parser->getData();
}

int HttpSession::sendResponse(HttpResponse::ptr rsp) {
    std::stringstream ss;
    ss << *rsp;
    std::string data = ss.str();
    return writeFixSize(data.c_str(), data.size());
}

}

}