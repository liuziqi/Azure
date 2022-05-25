#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include "azure.h"

azure::Logger::ptr g_logger = AZURE_LOG_ROOT();

void test_sleep() {
    azure::IOManager iom(1);
    
    // NOTE 用hook把同步变成异步：sleep换成超时操作
    iom.schedule([](){
        sleep(2);
        AZURE_LOG_INFO(g_logger) << "sleep 2";
    });

    iom.schedule([](){
        sleep(3);
        AZURE_LOG_INFO(g_logger) << "sleep 3";
    });

    AZURE_LOG_INFO(g_logger) << "test_sleep";
}

void test_sock() {
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);

    sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(80);
    inet_pton(AF_INET, "182.61.200.6", &addr.sin_addr.s_addr);

    AZURE_LOG_INFO(g_logger) << "begin connect";
    int rt = connect(sockfd, (const sockaddr*)&addr, sizeof(addr));
    AZURE_LOG_INFO(g_logger) << "connect rt=" << rt << " errno=" << errno;

    if(rt) {
        return;
    }

    const char data[] = "GET / HTTP/1.0\r\n\r\n";
    rt = send(sockfd, data, sizeof(data), 0);
    AZURE_LOG_INFO(g_logger) << "send rt=" << rt << " errno=" << errno;

    if(rt <= 0) {
        return;
    }

    std::string buf;
    buf.resize(4096);

    rt = recv(sockfd, &buf[0], buf.size(), 0);
    AZURE_LOG_INFO(g_logger) << "recv rt=" << rt << " errno=" << errno;

    if(rt <= 0) {
        return;
    }

    buf.resize(rt);
    AZURE_LOG_INFO(g_logger) << buf;
}

int main(int argc, char **argv) {
    test_sleep();
    // azure::IOManager iom;
    // iom.schedule(test_sock);
    return 0;
}