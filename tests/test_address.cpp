#include "address.h"
#include "log.h"
#include "netdb.h"

azure::Logger::ptr g_logger = AZURE_LOG_ROOT();

void test() {
    std::vector<azure::Address::ptr> addrs;


    bool v = azure::Address::Lookup(addrs, "www.baidu.com:http");
    // bool v = azure::Address::Lookup(addrs, "www.baidu.com:80");

    if(!v) {
        AZURE_LOG_ERROR(g_logger) << "lookup error";
        return;
    }

    for(size_t i = 0; i < addrs.size(); ++i) {
        AZURE_LOG_INFO(g_logger) << i << " - " << addrs[i]->toString();
    }
}

void test_iface() {
    std::multimap<std::string, std::pair<azure::Address::ptr, uint16_t>> results;
    bool v = azure::Address::GetInterfaceAddress(results);
    if(!v) {
        AZURE_LOG_ERROR(g_logger) << "GetInterfaceAddress fail";
        return;
    }

    for(auto &i : results) {
        AZURE_LOG_INFO(g_logger) << i.first << " - " << i.second.first->toString() << " - " << i.second.second;
    }
}

void test_ipv4() {
    // auto addr = azure::IPAddress::Create("www.sylar.top");
    auto addr = azure::IPAddress::Create("127.0.0.8");
    if(addr) {
        AZURE_LOG_INFO(g_logger) << addr->toString();
    }
}

int main(int argc, char **argv) {
    // test();
    // test_iface();
    test_ipv4();
    return 0;
}