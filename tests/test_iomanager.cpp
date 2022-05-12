#include "azure.h"
#include "iomanager.h"
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>

azure::Logger::ptr g_logger = AZURE_LOG_ROOT();

void test_fiber() {
    AZURE_LOG_INFO(g_logger) << "test_fibber";
}

void test1() {
    azure::IOManager iom;
    iom.schedule(&test_fiber);

    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    fcntl(sockfd, F_SETFL, O_NONBLOCK);

    sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(80);
    inet_pton(AF_INET, "182.61.200.6", &addr.sin_addr.s_addr);

    iom.addEvent(sockfd, azure::IOManager::READ, []() {
        AZURE_LOG_INFO(g_logger) << "connected READ";
    });
    iom.addEvent(sockfd, azure::IOManager::WRITE, []() {
        AZURE_LOG_INFO(g_logger) << "connected WRITE";
    });
    connect(sockfd, (const sockaddr*)&addr, sizeof(addr));

}

int main(int argc, char **argv) {
    test1();
    return 0;
}