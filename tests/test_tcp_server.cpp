#include "tcp_server.h"
#include "iomanager.h"
#include "log.h"

azure::Logger::ptr g_logger = AZURE_LOG_ROOT();

void run() {
    auto addr = azure::Address::LookupAny("0.0.0.0:8033");
    // auto addr2 = azure::UnixAddress::ptr(new azure::UnixAddress("/tmp/unix_addr"));
    // AZURE_LOG_INFO(g_logger) << *addr << " - " << *addr2;
    std::vector<azure::Address::ptr> addrs;

    addrs.push_back(addr);
    // addrs.push_back(addr2);

    azure::TcpServer::ptr tcp_server(new azure::TcpServer);
    std::vector<azure::Address::ptr> fails;

    while(!tcp_server->bind(addrs, fails)) {
        sleep(2);
    }
    tcp_server->start();
}

int main(int argc, char **argv) {
    azure::IOManager iom(2);
    iom.schedule(run);
    return 0;
}