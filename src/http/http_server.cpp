#include "http/http_server.h"
#include "log.h"

namespace azure {

namespace http {

static azure::Logger::ptr g_logger = AZURE_LOG_NAME("system");

HttpServer::HttpServer(bool keepalive, azure::IOManager *worker, azure::IOManager *accept_worker)
    : TcpServer(worker, accept_worker)
    , m_isKeepalive(keepalive) {
    m_dispatch.reset(new ServletDispatch);
}

void HttpServer::handleClient(Socket::ptr client) {
    azure::http::HttpSession::ptr session(new azure::http::HttpSession(client));
    do {
        auto req = session->recvRequest();
        if(!req) {
            AZURE_LOG_WARN(g_logger) << "recv http request fail, errno="
                                     << errno << " errstr=" << strerror(errno)
                                     << " client:" << *client;
            break;
        }

        HttpResponse::ptr rsp(new HttpResponse(req->getVersion(), req->isClosed() || !m_isKeepalive));
        m_dispatch->handle(req, rsp, session);

        // rsp->setBody("hello azure");
        // AZURE_LOG_INFO(g_logger) << "request:" << std::endl << *req;
        // AZURE_LOG_INFO(g_logger) << "response:" << std::endl << *rsp;

        session->sendResponse(rsp);

        if(!m_isKeepalive || req->isClosed()) {
            break;
        }
    } while(m_isKeepalive);
    session->close();
}

}

}