#ifndef __AZURE_TCP_SERVER_H__
#define __AZURE_TCP_SERVER_H__

#include <memory>
#include <functional>
#include <vector>
#include "iomanager.h"
#include "socket.h"
#include "address.h"
#include "noncopyable.h"

namespace azure {

class TcpServer : public std::enable_shared_from_this<TcpServer>, Noncpoyable {
public:
    typedef std::shared_ptr<TcpServer> ptr;

    TcpServer(azure::IOManager *worker=azure::IOManager::GetThis(), azure::IOManager *accept_worker=azure::IOManager::GetThis());
    virtual ~TcpServer();

    virtual bool bind(azure::Address::ptr addr);
    virtual bool bind(const std::vector<Address::ptr> &addrs, std::vector<Address::ptr> &fails);
    virtual bool start();
    virtual void stop();

    uint64_t getReadTimeout() const {return m_readTimeout;}
    std::string getName() const {return m_name;}

    void setReadTimeout(uint64_t v) {m_readTimeout = v;}
    void setName(const std::string &v) {m_name = v;}

    bool isStop() const {return m_isStop;}

protected:
    virtual void handleClient(Socket::ptr client);  // 处理监听连接事件
    virtual void startAccept(Socket::ptr sock);

private:
    std::vector<Socket::ptr> m_socks;   // 监听成功的socket
    IOManager *m_worker;
    IOManager *m_acceptWorker;
    uint64_t m_readTimeout;
    std::string m_name;
    bool m_isStop;                      // Server是否停止
};

}

#endif