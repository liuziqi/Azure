#ifndef __AZURE_HTTP_SERVER_H__
#define __AZURE_HTTP_SERVER_H__

#include <memory>
#include "tcp_server.h"
#include "http_session.h"
#include "servlet.h"
#include "iomanager.h"

namespace azure {

namespace http {

class HttpServer : public TcpServer {
public:
    typedef std::shared_ptr<HttpServer> ptr;

    HttpServer(bool keepalive=false, azure::IOManager *worker=azure::IOManager::GetThis()
                , azure::IOManager *accept_worker=azure::IOManager::GetThis());

    ServletDispatch::ptr getServletDispatch() const {return m_dispatch;}
    void setServletDispatch(ServletDispatch::ptr v) {m_dispatch = v;}

protected:
    virtual void handleClient(Socket::ptr client) override;

private:
    /**
     * @brief 
     * @details 
     * @exception 
     * @todo 
     * @note 
     */
    bool m_isKeepalive;
    ServletDispatch::ptr m_dispatch;
};

}

}

#endif