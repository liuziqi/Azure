#include <netinet/tcp.h>
#include <sys/types.h>
#include <sys/socket.h>
#include "socket.h"
#include "fdmanager.h"
#include "log.h"
#include "macro.h"
#include "hook.h"
#include "iomanager.h"

namespace azure {

static azure::Logger::ptr g_logger = AZURE_LOG_NAME("system");

Socket::ptr Socket::CreateTCP(azure::Address::ptr address) {
    Socket::ptr sock(new Socket(address->getFamily(), Socket::TCP, 0));
    return sock;
}

Socket::ptr Socket::CreateUDP(azure::Address::ptr address) {
    Socket::ptr sock(new Socket(address->getFamily(), Socket::UDP, 0));
    return sock;
}

Socket::ptr Socket::CreateTCPSocket() {
    Socket::ptr sock(new Socket(Socket::IPv4, Socket::TCP, 0));
    return sock;
}

Socket::ptr Socket::CreateUDPSocket() {
    Socket::ptr sock(new Socket(Socket::IPv4, Socket::UDP, 0));
    return sock;
}

Socket::ptr Socket::CreateTCPSocket6() {
    Socket::ptr sock(new Socket(Socket::IPv6, Socket::TCP, 0));
    return sock;
}

Socket::ptr Socket::CreateUDPSocket6() {
    Socket::ptr sock(new Socket(Socket::IPv6, Socket::UDP, 0));
    return sock;
}

Socket::ptr Socket::CreateTCPUnixSocket() {
    Socket::ptr sock(new Socket(Socket::UNIX, Socket::TCP, 0));
    return sock;
}

Socket::ptr Socket::CreateUDPUnixSocket() {
    Socket::ptr sock(new Socket(Socket::UNIX, Socket::UDP, 0));
    return sock;
}

Socket::Socket(int family, int type, int protocol)
    : m_sockfd(-1)
    , m_family(family)
    , m_type(type)
    , m_protocol(protocol)
    , m_isConnected(false) {
}

Socket::~Socket() {
    close();
}

int64_t Socket::getSendTimeout() {
    FdCtx::ptr ctx = FdMgr::GetInstance()->get(m_sockfd);
    if(ctx) {
        return ctx->getTimeout(SO_SNDTIMEO);
    }
    return -1;
}

void Socket::setSendTimeout(uint64_t v) {
    struct timeval tv{int(v / 1000), int(v % 1000 * 1000)};
    setOption(SOL_SOCKET, SO_SNDTIMEO, tv);
}

int64_t Socket::getRecvTimeout() {
    FdCtx::ptr ctx = FdMgr::GetInstance()->get(m_sockfd);
    if(ctx) {
        return ctx->getTimeout(SO_RCVTIMEO);
    }
    return -1;
}

void Socket::setRecvTimeout(uint64_t v) {
    struct timeval tv{int(v / 1000), int(v % 1000 * 1000)};
    setOption(SOL_SOCKET, SO_RCVTIMEO, tv);
}

bool Socket::getOption(int level, int option, void *result, size_t *len) {
    int rt = getsockopt(m_sockfd, level, option, result, (socklen_t*)len);
    if(rt) {
        AZURE_LOG_DEBUG(g_logger) << "getOption sockfd=" << m_sockfd << " level=" << level << " option=" << option
                                    << " errno=" << errno << " errstr=" << strerror(errno);
        return false;
    }
    return true;
}

bool Socket::setOption(int level, int option, const void *result, size_t len) {
    if(setsockopt(m_sockfd, level, option, result, (socklen_t)len)) {
        AZURE_LOG_DEBUG(g_logger) << "getOption sockfd=" << m_sockfd << " level=" << level << " option=" << option
                                    << " errno=" << errno << " errstr=" << strerror(errno);
        return false;
    }
    return true;
}

Socket::ptr Socket::accept() {
    Socket::ptr sock(new Socket(m_family, m_type, m_protocol));
    int newsock = ::accept(m_sockfd, nullptr, nullptr);
    if(newsock == -1) {
        AZURE_LOG_ERROR(g_logger) << "accept(" << m_sockfd << ") errno=" << errno << " errstr=" << strerror(errno);
        return nullptr;
    }
    if(sock->init(newsock)) {
        return sock;
    }
    return nullptr;
}

bool Socket::init(int sockfd) {
    FdCtx::ptr ctx = FdMgr::GetInstance()->get(sockfd);
    if(ctx && ctx->isSocket() && !ctx->isClosed()) {
        m_sockfd = sockfd;
        m_isConnected = true;
        initSock();
        getLocalAddress();
        getRemoteAddress();
        return true;
    }
    return false;
}

bool Socket::bind(const Address::ptr addr) {
    if(!isValid()) {
        newSock();
        if(AZURE_UNLIKELY(!isValid())) {
            return false;
        }
    }

    if(AZURE_UNLIKELY(addr->getFamily() != m_family)) {
        AZURE_LOG_ERROR(g_logger) << "bind sock.family (" << m_family << ") addr.family(" << addr->getFamily() << ") not equal, addr=" << addr->toString();
        return false;
    }

    if(::bind(m_sockfd, addr->getAddr(), addr->getAddrLen())) {
        AZURE_LOG_ERROR(g_logger) << "bind error, errno=" << errno << " errstr=" << strerror(errno);
        return false;
    }
    getLocalAddress();
    return true;
}

bool Socket::connect(const Address::ptr addr, uint64_t timeout_ms) {
    if(!isValid()) {
        newSock();
        if(AZURE_UNLIKELY(!isValid())) {
            return false;
        }
    }

    if(AZURE_UNLIKELY(addr->getFamily() != m_family)) {
        AZURE_LOG_ERROR(g_logger) << "connect sock.family (" << m_family << ") addr.family(" << addr->getFamily() << ") not equal, addr=" << addr->toString();
        return false;
    }

    if(timeout_ms == (uint64_t)-1) {
        if(::connect(m_sockfd, addr->getAddr(), addr->getAddrLen())) {
            AZURE_LOG_ERROR(g_logger) << "sock=" << m_sockfd << " connect(" << addr->toString() << ") error errno=" 
                                        << errno << " errstr=" << strerror(errno);
            close();
            return false;
        }
    }
    else {
        if(::connect_with_timeout(m_sockfd, addr->getAddr(), addr->getAddrLen(), timeout_ms)) {
            AZURE_LOG_ERROR(g_logger) << "sock=" << m_sockfd << " connect(" << addr->toString()
                                        << ") timeout=" << timeout_ms << " error errno=" << errno << " errstr=" << strerror(errno);
        }
        close();
        return false;
    }
    m_isConnected = true;
    getRemoteAddress();
    getLocalAddress();
    return true;
}

bool Socket::listen(int backlog) {
    if(!isValid()) {
        AZURE_LOG_ERROR(g_logger) << "listen error";
        return false;
    }
    if(::listen(m_sockfd, backlog)) {
        AZURE_LOG_ERROR(g_logger) << "listen error errno=" << errno << " errstr=" << strerror(errno);
        return false;
    }
    return true;
}

bool Socket::close() {
    if(!m_isConnected && m_sockfd == -1) {
        return true;
    }
    m_isConnected = false;
    if(m_sockfd == -1) {
        ::close(m_sockfd);
        m_sockfd = -1;
    }
    return false;
}

// ssize_t send(int sockfd, const void *buf, size_t len, int flags)
int Socket::send(const void *buffer, size_t length, int flags) {
    if(isConnected()) {
        return  ::send(m_sockfd, buffer, length, flags);
    }
    return -1;
}

// ssize_t sendmsg(int sockfd, const msghdr *msg, int flags)
int Socket::send(const iovec *buffers, size_t length, int flags) {
    if(isConnected()) {
        msghdr msg;
        memset(&msg, 0, sizeof(msg));
        msg.msg_iov = (iovec*)buffers;
        msg.msg_iovlen = length;
        return ::sendmsg(m_sockfd, &msg, flags);
    }   
    return -1;
}

// sendto(int sockfd, const void *buf, size_t len, int flags, const sockaddr *dest_addr, socklen_t addrlen)
int Socket::sendTo(const void *buffer, size_t length, const Address::ptr to, int flags) {
    if(isConnected()) {
        return ::sendto(m_sockfd, buffer, length, flags, to->getAddr(), to->getAddrLen());
    }
    return -1;
}

// sendmsg(int sockfd, const msghdr *msg, int flags)
int Socket::sendTo(const iovec *buffers, size_t length, const Address::ptr to, int flags) {
    if(isConnected()) {
        msghdr msg;
        memset(&msg, 0, sizeof(msg));
        msg.msg_iov = (iovec*)buffers;
        msg.msg_iovlen = length;
        msg.msg_name = to->getAddr();
        msg.msg_namelen = to->getAddrLen();
        return ::sendmsg(m_sockfd, &msg, flags);
    }
    return -1;
}

// ssize_t recv(int sockfd, void *buf, size_t len, int flags)
int Socket::recv(void *buffer, size_t length, int flags) {
    if(isConnected()) {
        return ::recv(m_sockfd, buffer, length, flags);
    }
    return -1;
}

// ssize_t recvmsg(int sockfd, msghdr *msg, int flags)
int Socket::recv(iovec *buffers, size_t length, int flags) {
    if(isConnected()) {
        msghdr msg;
        memset(&msg, 0, sizeof(msg));
        msg.msg_iov = (iovec*)buffers;
        msg.msg_iovlen = length;
        return ::recvmsg(m_sockfd, &msg, flags); 
    }
    return -1;
}

// ssize_t recvfrom(int sockfd, void *buf, size_t len, int flags, sockaddr *src_addr, socklen_t *addrlen)
int Socket::recvFrom(void *buffer, size_t length, Address::ptr from, int flags) {
    if(isConnected()) {
        socklen_t len = from->getAddrLen();
        return ::recvfrom(m_sockfd, buffer, length, flags, from->getAddr(), &len);
    }
    return -1;
}

int Socket::recvFrom(iovec *buffers, size_t length, Address::ptr from, int flags) {
    if(isConnected()) {
        msghdr msg;
        memset(&msg, 0, sizeof(msg));
        msg.msg_iov = (iovec*)buffers;
        msg.msg_iovlen = length;
        msg.msg_name = from->getAddr();
        msg.msg_namelen = from->getAddrLen();
        return ::recvmsg(m_sockfd, &msg, flags);
    }
    return -1;
}

Address::ptr Socket::getRemoteAddress() {
    if(m_remoteAddress) {
        return m_remoteAddress;
    }

    Address::ptr result;
    switch(m_family) {
    case AF_INET:
        result.reset(new IPv4Address());
        break;
    case AF_INET6:
        result.reset(new IPv6Address());
        break;
    case AF_UNIX:
        result.reset(new UnixAddress());
        break;
    default:
        result.reset(new UnknowAddress(m_family));
        break;
    }
    socklen_t addrlen = result->getAddrLen();
    if(getpeername(m_sockfd, result->getAddr(), &addrlen)) {
        AZURE_LOG_ERROR(g_logger) << "getpeername error sock=" << m_sockfd << " errno=" << errno << " errstr=" << strerror(errno);
        return Address::ptr(new UnknowAddress(m_family));
    }
    if(m_family == AF_UNIX) {
        UnixAddress::ptr addr = std::dynamic_pointer_cast<UnixAddress>(result);
        addr->setAddrLen(addrlen);
    }
    m_remoteAddress = result;
    return m_remoteAddress;
}

Address::ptr Socket::getLocalAddress() {
    if(m_localAddress) {
        return m_localAddress;
    }

    Address::ptr result;
    switch(m_family) {
    case AF_INET:
        result.reset(new IPv4Address());
        break;
    case AF_INET6:
        result.reset(new IPv6Address());
        break;
    case AF_UNIX:
        result.reset(new UnixAddress());
        break;
    default:
        result.reset(new UnknowAddress(m_family));
        break;
    }
    socklen_t addrlen = result->getAddrLen();
    if(getsockname(m_sockfd, result->getAddr(), &addrlen)) {
        AZURE_LOG_ERROR(g_logger) << "getsockname error sock=" << m_sockfd << " errno=" << errno << " errstr=" << strerror(errno);
        return Address::ptr(new UnknowAddress(m_family));
    }
    if(m_family == AF_UNIX) {
        UnixAddress::ptr addr = std::dynamic_pointer_cast<UnixAddress>(result);
        addr->setAddrLen(addrlen);
    }
    m_localAddress = result;
    return m_localAddress;
}

bool Socket::isValid() const {
    return m_sockfd != -1;
}

int Socket::getError() {
    int error = 0;
    size_t len = sizeof(error);
    if(!getOption(SOL_SOCKET, SO_ERROR, &error, &len)) {
        return -1;
    }
    return error;
}

std::ostream &Socket::dump(std::ostream &os) const {
     os << "[Socket sockfd=" << m_sockfd
        << " is_connected=" << m_isConnected
        << " family=" << m_family
        << " type=" << m_type
        << " protocol=" << m_protocol;
    if(m_localAddress) {
        os << " locl_address=" << m_localAddress->toString();
    }
    if(m_remoteAddress) {
        os << " m_remoteAddress=" << m_remoteAddress->toString();
    }
    os << "]";
    return os;
}

// 这些函数是阻塞的，有时候需要取消掉
bool Socket::cancelRead() {
    return IOManager::GetThis()->cancelEvent(m_sockfd, azure::IOManager::READ);
}

bool Socket::cancelWrite() {
    return IOManager::GetThis()->cancelEvent(m_sockfd, azure::IOManager::WRITE);
}

bool Socket::cancelAccept() {
    return IOManager::GetThis()->cancelEvent(m_sockfd, azure::IOManager::READ);
}

bool Socket::cancelAll() {
    return IOManager::GetThis()->cancelAll(m_sockfd);
}

void Socket::initSock() {
    int val = 1;
    setOption(SOL_SOCKET, SO_REUSEADDR, val);
    if(m_type == SOCK_STREAM) {
        setOption(IPPROTO_TCP, TCP_NODELAY, val);   // 是否开启Nagle算法
    }
}

void Socket::newSock() {
    m_sockfd = socket(m_family, m_type, m_protocol);
    if(AZURE_LIKELY(m_sockfd != -1)) {              // 告诉编译器该预测分支发生的可能性大
        initSock();
    }
    else {
        AZURE_LOG_ERROR(g_logger) << "socket(" << m_family << ", " << m_type << ", " << m_protocol << ") errno=" << errno << " errstr=" << strerror(errno);
    }
}

std::ostream &operator<<(std::ostream &os, const Socket &sock) {
    return sock.dump(os);
}

}