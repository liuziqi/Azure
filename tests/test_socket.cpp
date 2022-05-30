#include "socket.h"
#include "address.h"
#include "azure.h"

azure::Logger::ptr g_logger = AZURE_LOG_ROOT();

void test_socket() {
    azure::IPAddress::ptr addr = azure::Address::LookupAnyIPAddress("www.baidu.com");
    if(addr) {
        AZURE_LOG_INFO(g_logger) << "get address: " << addr->toString();
    }
    else {
        AZURE_LOG_ERROR(g_logger) << "get address fial";
    }

    azure::Socket::ptr sock = azure::Socket::CreateTCP(addr);
    addr->setPort(80);
    if(sock->connect(addr)) {
        AZURE_LOG_ERROR(g_logger) << "connect " << addr->toString() << " connected";
    }
    else {
        AZURE_LOG_INFO(g_logger) << "connect " << addr->toString() << " fail";
    }

    const char buf[] = "GET / HTTP/1.0\r\n\r\n";
    int rt = sock->send(buf, sizeof(buf));
    if(rt <= 0) {
        AZURE_LOG_INFO(g_logger) << " send fail rt=" << rt;
        return;
    }

    std::string buffer;
    buffer.resize(4096);
    rt = sock->recv(&buffer[0], buffer.size());

    if(rt <= 0) {
        AZURE_LOG_INFO(g_logger) << " recv fail rt=" << rt;
        return;
    }

    buffer.resize(rt);
    AZURE_LOG_INFO(g_logger) << buffer;
}

int main(int argc, char **argv) {
    azure::IOManager iom;
    iom.schedule(&test_socket);
    return 0;
}