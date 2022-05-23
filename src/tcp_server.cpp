#include "tcp_server.h"
#include "config.h"
#include "log.h"

namespace azure {

static Logger::ptr g_logger = AZURE_LOG_NAME("system");

static ConfigVar<uint64_t>::ptr g_tcp_server_read_timeout = Config::Lookup("tcp_server.read_timeout", (uint64_t)(60 * 1000 * 2), "tcp server read timeout");

TcpServer::TcpServer(azure::IOManager *worker, azure::IOManager *accept_worker)
    : m_worker(worker)
    , m_acceptWorker(accept_worker)
    , m_readTimeout(g_tcp_server_read_timeout->getValue())
    , m_name("azure/1.0.0")
    , m_isStop(true) {
}

TcpServer::~TcpServer() {
    for(auto &i : m_socks) {
        i->close();
    }
    m_socks.clear();
}

bool TcpServer::bind(azure::Address::ptr addr) {
    std::vector<Address::ptr> addrs;
    std::vector<Address::ptr> fails;
    addrs.push_back(addr);
    return bind(addrs, fails);
}

// 返回bind失败的地址
bool TcpServer::bind(const std::vector<Address::ptr> &addrs, std::vector<Address::ptr> &fails) {
    for(auto &addr : addrs) {
        Socket::ptr sock = Socket::CreateTCP(addr);
        if(!sock->bind(addr)) {
            AZURE_LOG_ERROR(g_logger) << "bind fail, errno="
                                      << errno << ", errstr=" << strerror(errno)
                                      << " addr=[" << addr->toString() << "]";
            fails.push_back(addr);
            continue;
        }
        if(!sock->listen()) {
            AZURE_LOG_ERROR(g_logger) << "listen fail errno="
                                    << errno << " errstr="
                                    << strerror(errno)
                                    << " addr=[" << addr->toString() << "]";
            fails.push_back(addr);
            continue;
        }
        m_socks.push_back(sock);
    }

    if(!fails.empty()) {
        m_socks.clear();
        return false;
    }

    for(auto &i : m_socks) {
        AZURE_LOG_INFO(g_logger) << "server bind success: " << *i;
    }
    return true;
}

void TcpServer::startAccept(Socket::ptr sock) {
    while(!m_isStop) {
        Socket::ptr client = sock->accept();
        if(client) {
            client->setRecvTimeout(m_readTimeout);
            m_worker->schedule(std::bind(&TcpServer::handleClient, shared_from_this(), client));
        }
        else {
            AZURE_LOG_ERROR(g_logger) << "accept errno=" << errno << " errstr=" << strerror(errno);
        }
    }
}

// 开启 accept
bool TcpServer::start() {
    if(!m_isStop) {
        return true;
    }
    m_isStop = false;
    for(auto &sock : m_socks) {
        m_acceptWorker->schedule(std::bind(&TcpServer::startAccept, shared_from_this(), sock));
    }
    return true;
}

void TcpServer::stop() {
    m_isStop = true;
    auto self = shared_from_this();
    m_acceptWorker->schedule([this, self](){
        for(auto &sock : m_socks) {
            sock->cancelAll();
            sock->close();
        }
        m_socks.clear();
    });
}

void TcpServer::handleClient(Socket::ptr client) {
    AZURE_LOG_INFO(g_logger) << "handleClient: " << *client;
}

}