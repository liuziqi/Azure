#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include "azure.h"
#include "iomanager.h"

azure::Logger::ptr g_logger = AZURE_LOG_ROOT();

int sockfd = 0;

void test_fiber() {
    AZURE_LOG_INFO(g_logger) << "test_fibber sockfd=" << sockfd;

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    fcntl(sockfd, F_SETFL, O_NONBLOCK);

    sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(80);
    inet_pton(AF_INET, "182.61.200.6", &addr.sin_addr.s_addr);

    if(!connect(sockfd, (const sockaddr*)&addr, sizeof(addr))) {
        ;
    }
    else if(errno == EINPROGRESS) {
        AZURE_LOG_INFO(g_logger) << "add event errno=" << errno << strerror(errno);

        azure::IOManager::GetThis()->addEvent(sockfd, azure::IOManager::READ, []() {
            AZURE_LOG_INFO(g_logger) << "READ callback";
        });

        azure::IOManager::GetThis()->addEvent(sockfd, azure::IOManager::WRITE, [&]() {
            AZURE_LOG_INFO(g_logger) << "WRITE callback";
            azure::IOManager::GetThis()->cancelEvent(sockfd, azure::IOManager::READ);
            close(sockfd);
        });
    }
    else {
        AZURE_LOG_INFO(g_logger) << "else " << errno << " " << strerror(errno);
    }
}

void test1() {
    azure::IOManager iom(1);
    // azure::IOManager iom(2, false);
    iom.schedule(&test_fiber);
}

void test_timer() {
    azure::IOManager iom(2);
    azure::Timer::ptr timer = iom.addTimer(1000, [&](){
        static int i = 0; 
        AZURE_LOG_INFO(g_logger) << "hello timer i=" << i;        
        if(++i == 5) {
            timer->cancel();
        }
    }, true);
}

int main(int argc, char **argv) {
    test1();
    // test_timer();
    return 0;
}