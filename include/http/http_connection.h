#ifndef __AZURE_HTTP_CONNECTION_H__
#define __AZURE_HTTP_CONNECTION_H__

#include <memory>
#include "socket_stream.h"
#include "http/http.h"

namespace azure {

namespace http {

class HttpConnection : public SocketStream {
public:
    typedef std::shared_ptr<HttpConnection> ptr;

    HttpConnection(Socket::ptr sock, bool owner=true);

    HttpResponse::ptr recvResponse();
    int sendRequest(HttpRequest::ptr req);
};

}

}

#endif