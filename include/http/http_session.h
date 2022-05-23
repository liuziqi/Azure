#ifndef __AZURE_HTTP_SESSION_H__
#define __AZURE_HTTP_SESSION_H__

#include <memory>
#include "socket_stream.h"
#include "http/http.h"

namespace azure {

namespace http {

class HttpSession : public SocketStream {
public:
    typedef std::shared_ptr<HttpSession> ptr;

    HttpSession(Socket::ptr sock, bool owner=true);

    HttpRequest::ptr recvRequest();
    int sendResponse(HttpResponse::ptr rsp);
};

}

}

#endif