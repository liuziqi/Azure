#include <sstream>
#include <netdb.h>
#include "address.h"
#include "endian_.h"
#include "log.h"

namespace azure {

static azure::Logger::ptr g_logger = AZURE_LOG_ROOT();

template<typename T>
static T CreateMask(uint32_t bits) {
    return (1 << (sizeof(T) * 8 - bits)) - 1;
}

Address::ptr Address::Create(const sockaddr *addr, socklen_t addrlen) {
    if(addr == nullptr) {
        return nullptr;
    }

    Address::ptr result;
    switch(name->sa_family) {
    case AF_INET:
        result.reset(new IPv4Address(*(const sockaddr_in*)addr));
        break;
    case AF_INET6:
        result.reset(new IPv6Address(*(const sockaddr_in6*)addr));
        break;
    default:
        result.reset(new UnknowAddress(*addr));
        break;
    }
}

bool Address::Lookup(std::vector<Address::ptr> &result, const std::string &host, int family, int type, int protocol) {
    addrinfo hints, *result, *next;
    hints.ai_flags = 0;
    hints.ai_family = family;
    hints.ai_socktype = type;
    hints.ai_protocol = protocol;
    hints.ai_addr = 0;
    hints.ai_canonname = nullptr;
    hints.ai_addr = nullptr;
    hints.ai_next = nullptr;

    std::string node;
    const char *service = nullptr;

    if(!host.empty() && host[0] == '[') {
        const char *endipv6 = (const char *)memchr(host.c_str() + 1, "]", host.size() - 1);
    }
}

int Address::getFamily() const {
    return getAddr()->sa_family;
}

std::string Address::toString() {
    std::stringstream ss;
    insert(ss);
    return ss.str();
}

bool Address::operator<(const Address &rhs) const {
    socklen_t minlen = std::min(getAddrLen(), rhs.getAddrLen());
    int result = memcmp(getAddr(), rhs.getAddr(),  minlen);
    if(result == 0) {
        return getAddrLen() < rhs.getAddrLen();
    }
    return result < 0;
}

bool Address::operator==(const Address &rhs) const {
    return getAddrLen() == rhs.getAddrLen() && memcmp(getAddr(), rhs.getAddr(), getAddrLen()) == 0;
}

bool Address::operator!=(const Address &rhs) const {
    return !(*this == rhs);
}

// 将域名转换为IP地址
IPAddress::ptr IPAddress::Create(const char *address, uint32_t port=0) {
    addrinfo hints, *results;
    memset(&hints, 0, sizeof(hints));

    hints.ai_flags = AI_NUMERICHOST;
    hints.ai_family = AF_UNSPEC;

    int error = getaddrinfo(address, NULL, &hints, &results);
    if(error) {
        AZURE_LOG_ERROR(g_logger) << "IPAddress::Create(" << address << ", " << port << ") error=" << error << " errno=" << errno << " errstr=" << strerror(errno);
        return nullptr;
    }

    try {
        IPAddress::ptr result = std::dynamic_pointer_cast<IPAddress>(Address::Create(results->ai_addr, (socklen_t)results->ai_addrlen));
        if(result) {
            result->setPort(port);
        }
        freeaddrinfo(results);
        return result;
    }
    catch(...) {
        freeaddrinfo(results);
        return nullptr;
    }
}

IPv4Address::ptr IPv4Address::Create(const char *address, uint32_t port) {
    IPv4Address::ptr rt(new IPv4Address);
    rt->m_addr.sin_port = byteswapOnLittleEndian(port);
    int result = inet_pton(AF_INET, address, &rt->m_addr.sin_addr);
    if(result <= 0) {
        AZURE_LOG_ERROR(g_logger) << "IPv4Address::Create(" << address << ", " << port << "( rt=" << result << " errno=" << errno << " errstr=" << strerror(errno);
        return nullptr;
    }
    return rt;
}

IPv4Address::IPv4Address(const sockaddr_in &address) {
    m_addr = address;
}

IPv4Address::IPv4Address(uint32_t address, uint32_t port) {
    memset(&m_addr, 0, sizeof(m_addr));
    m_addr.sin_family = AF_INET;
    m_addr.sin_port = byteswapOnLittleEndian(port);
    m_addr.sin_addr.s_addr = byteswapOnLittleEndian(address);
}

const sockaddr *IPv4Address::getAddr() const {
    return (sockaddr*)&m_addr;
}

socklen_t IPv4Address::getAddrLen() const {
    return sizeof(m_addr);
}

std::ostream &IPv4Address::insert(std::ostream &os) const  {
    uint32_t addr = byteswapOnLittleEndian(m_addr.sin_addr.s_addr);
    os << ((addr >> 24) & 0xff) << "." << ((addr >> 16) & 0xff) << "." << ((addr >> 8) & 0xff) << "." << (addr & 0xff);     // 转换成点分十进制
    os << ":" << byteswapOnLittleEndian(m_addr.sin_port);
    return os;
}

IPAddress::ptr IPv4Address::broadcastAddress(uint32_t prefix_len) {
    if(prefix_len > 32) {
        return nullptr;
    }
    sockaddr_in bcaddr(m_addr);
    bcaddr.sin_addr.s_addr |= byteswapOnLittleEndian(CreateMask<uint32_t>(prefix_len));
    return IPv4Address::ptr(new IPv4Address(bcaddr));
}

IPAddress::ptr IPv4Address::networkAddress(uint32_t prefix_len) {
    if(prefix_len > 32) {
        return nullptr;
    }
    sockaddr_in nwaddr(m_addr);
    nwaddr.sin_addr.s_addr &= ~byteswapOnLittleEndian(CreateMask<uint32_t>(prefix_len));
    return IPv4Address::ptr(new IPv4Address(nwaddr));
}

IPAddress::ptr IPv4Address::subnetMask(uint32_t prefix_len) {
    sockaddr_in subnet;
    memset(&subnet, 0, sizeof(subnet));
    subnet.sin_family = AF_INET;
    subnet.sin_addr.s_addr = ~byteswapOnLittleEndian(CreateMask<uint32_t>(prefix_len));
    return IPv4Address::ptr(new IPv4Address(subnet));
}

uint32_t IPv4Address::getPort() const {
    return byteswapOnLittleEndian(m_addr.sin_port);
}

void IPv4Address::setPort(uint32_t v) {
    m_addr.sin_port = byteswapOnLittleEndian(v);
}

IPv6Address::ptr IPv6Address::Create(const char *address, uint32_t port) {
    IPv6Address::ptr rt(new IPv6Address);
    rt->m_addr.sin6_port = byteswapOnLittleEndian(port);
    int result = inet_pton(AF_INET6, address, &rt->m_addr.sin6_addr);
    if(result <= 0) {
        AZURE_LOG_ERROR(g_logger) << "IPv6Address::Create(" << address << ", " << port << "( rt=" << result << " errno=" << errno << " errstr=" << strerror(errno);
        return nullptr;
    }
    return rt;
}

IPv6Address::IPv6Address() {
    memset(&m_addr, 0, sizeof(m_addr));
    m_addr.sin6_family = AF_INET6;
}

IPv6Address::IPv6Address(const sockaddr_in6 &address) {
    m_addr = address;
}

IPv6Address::IPv6Address(const uint8_t address[16], uint32_t port) {
    memset(&m_addr, 0, sizeof(m_addr));
    m_addr.sin6_family = AF_INET6;
    m_addr.sin6_port = byteswapOnLittleEndian(port);
    memcpy(&m_addr.sin6_addr.s6_addr, address, 16);
}

const sockaddr *IPv6Address::getAddr() const {
    return (sockaddr*)&m_addr;
}

socklen_t IPv6Address::getAddrLen() const {
    return sizeof(m_addr);
}

std::ostream &IPv6Address::insert(std::ostream &os) const  {
    os << "[";
    uint16_t *addr = (uint16_t*)m_addr.sin6_addr.s6_addr;
    bool used_zeros = false;
    for(size_t i = 0; i < 8; ++i) {
        if(addr[i] == 0 && !used_zeros) {
            continue;
        }
        if(i && addr[i - 1] == 0 && !used_zeros) {
            os << ":";
            used_zeros = true;
        }
        if(i) {
            os << ":";
        }
        os << (int)byteswapOnLittleEndian(addr[i]);
    }

    if(!used_zeros && addr[7] == 0) {
        os << "::";
    }

    os << "]:" << byteswapOnLittleEndian(m_addr.sin6_port);
    return os;
}

IPAddress::ptr IPv6Address::broadcastAddress(uint32_t prefix_len) {
    sockaddr_in6 bcaddr(m_addr);
    bcaddr.sin6_addr.s6_addr[prefix_len / 8] |= CreateMask<uint8_t>(prefix_len % 8);
    for(int i = prefix_len / 8 + 1; i < 16; ++i) {
        bcaddr.sin6_addr.s6_addr[i] = 0xff;
    }
    return IPv6Address::ptr(new IPv6Address(bcaddr));
}

IPAddress::ptr IPv6Address::networkAddress(uint32_t prefix_len) {
    sockaddr_in6 bcaddr(m_addr);
    bcaddr.sin6_addr.s6_addr[prefix_len / 8] &= CreateMask<uint8_t>(prefix_len % 8);
    return IPv6Address::ptr(new IPv6Address(bcaddr));
}

IPAddress::ptr IPv6Address::subnetMask(uint32_t prefix_len) {
    sockaddr_in6 subnet(m_addr);
    memset(&subnet, 0, sizeof(subnet));
    subnet.sin6_family = AF_INET6;
    subnet.sin6_addr.s6_addr[prefix_len / 8] = ~CreateMask<uint8_t>(prefix_len % 8);
    for(uint32_t i = 0; i < prefix_len / 8; ++i) {
        subnet.sin6_addr.s6_addr[i] = 0xff;
    }
    return IPv6Address::ptr(new IPv6Address(subnet));
}

uint32_t IPv6Address::getPort() const {
    return byteswapOnLittleEndian(m_addr.sin6_port);
}

void IPv6Address::setPort(uint32_t v) {
    m_addr.sin6_port = byteswapOnLittleEndian(v);
}

static const size_t MAX_PATH_LEN = sizeof(((sockaddr_un*)0)->sun_path) - 1;

UnixAddress::UnixAddress() {
    memset(&m_addr, 0, sizeof(m_addr));
    m_addr.sun_family = AF_UNIX;
    m_length = offsetof(sockaddr_un, sun_path) + MAX_PATH_LEN;
}

UnixAddress::UnixAddress(const std::string &path) {
    memset(&m_addr, 0, sizeof(m_addr));
    m_addr.sun_family = AF_UNIX;
    m_length = path.size() + 1;

    if(!path.empty() && path[0] == '\0') {
        --m_length;
    }

    if(m_length > sizeof(m_addr.sun_path)) {
        throw std::logic_error("path too long");
    }
    memcmp(m_addr.sun_path, path.c_str(), m_length);
    m_length += offsetof(sockaddr_un, sun_path);
}

const sockaddr *UnixAddress::getAddr() const {
    return (sockaddr*)&m_addr;
}

socklen_t UnixAddress::getAddrLen() const {
    return m_length;
}

std::ostream &UnixAddress::insert(std::ostream &os) const  {
    if(m_length > offsetof(sockaddr_un, sun_path) && m_addr.sun_path[0] == '\0') {
        return os << "\\0" << std::string(m_addr.sun_path + 1, m_length - offsetof(sockaddr_un, sun_path) - 1);
    }
    return os << m_addr.sun_path;
}

UnknowAddress::UnknowAddress(int family) {
    memset(&m_addr, 0, sizeof(m_addr));
    m_addr.sa_family = family;
}

UnknowAddress::UnknowAddress(const sockaddr &address) {
    m_addr = address;
}

const sockaddr *UnknowAddress::getAddr() const {
    return &m_addr;
}

socklen_t UnknowAddress::getAddrLen() const {
    return sizeof(m_addr);
}

std::ostream &UnknowAddress::insert(std::ostream &os) const  {
    os << "[UnknownAddress family=" << m_addr.sa_family << "]";
    return os;
}

}