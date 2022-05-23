#include "tcp_server.h"
#include "log.h"
#include "iomanager.h"
#include "bytearray.h"

static azure::Logger::ptr g_logger = AZURE_LOG_ROOT();

class EchoServer : public azure::TcpServer {
public:
    // type: 文本信息还是二进制信息
    EchoServer(int type);
    void handleClient(azure::Socket::ptr client) override;

private:
    int m_type = 0;
};

EchoServer::EchoServer(int type)
    : m_type(type) {
}

void EchoServer::handleClient(azure::Socket::ptr client) {
    AZURE_LOG_INFO(g_logger) << "handleClient " << *client;
    azure::ByteArray::ptr ba(new azure::ByteArray);
    while(true) {
        ba->clear();
        std::vector<iovec> iovs;
        ba->getWriteBuffers(iovs, 1024);

        int rt = client->recv(&iovs[0], iovs.size());
        if(rt == 0) {
            AZURE_LOG_INFO(g_logger) << "client close: " << *client;
            break;
        }
        else if(rt < 0) {
            AZURE_LOG_INFO(g_logger) << "client error rt=" << rt << " errno=" << errno << " errstr=" << strerror(errno);
            break;
        }
        ba->setPosition(ba->getPosition() + rt);
        ba->setPosition(0);
        // AZURE_LOG_INFO(g_logger) << "recv rt=" << rt << " data=" << std::string((char*)iovs[0].iov_base, rt);
        if(m_type == 1) {   // text
            // AZURE_LOG_INFO(g_logger) << ba->toString();
            std::cout << ba->toString();
        }
        else {
            // AZURE_LOG_INFO(g_logger) << ba->toHexString();
            std::cout << ba->toHexString();
        }
        std::cout.flush();
    }
}

int type = 1;

void run() {
    EchoServer::ptr es(new EchoServer(type));
    auto addr = azure::Address::LookupAny("0.0.0.0:8020");
    while(!es->bind(addr)) {
        sleep(2);
    }
    es->start();
}

int main(int argc, char **argv) {
    if(argc < 2) {
        AZURE_LOG_INFO(g_logger) << "used as[" << argv[0] << " -t] or [" << argv[0] << " -b]";
        return 0;
    }

    if(!strcmp(argv[1], "-b")) {
        type = 2;
    }

    azure::IOManager iom(2);
    iom.schedule(&run);
    return 0;
}