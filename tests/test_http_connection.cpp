#include <iostream>
#include "http/http_connection.h"
#include "iomanager.h"
#include "log.h"

azure::Logger::ptr g_logger = AZURE_LOG_ROOT();

void run() {
    azure::Address::ptr addr = azure::Address::LookupAnyIPAddress("www.sylar.top:80");
    if(!addr) {
        AZURE_LOG_ERROR(g_logger) << "get addr error";
        return;
    }

    azure::Socket::ptr sock = azure::Socket::CreateTCP(addr);
    bool rt = sock->connect(addr);
    if(!rt) {
        AZURE_LOG_INFO(g_logger) << "connect " << *addr << " failed";
        return;
    }

    azure::http::HttpConnection::ptr conn(new azure::http::HttpConnection(sock));

    azure::http::HttpRequest::ptr req(new azure::http::HttpRequest);
    req->setPath("/blog/");
    req->setHeader("Host", "www.sylar.top");
    AZURE_LOG_INFO(g_logger) << "req: " << std::endl << *req;

    conn->sendRequest(req);
    auto rsp = conn->recvResponse();

    if(!rsp) {
        AZURE_LOG_INFO(g_logger) << "recv response error";
        return;
    }
    AZURE_LOG_INFO(g_logger) << "rsp: " << std::endl << *rsp;

    AZURE_LOG_INFO(g_logger) << "===============================================";

    auto rt2 = azure::http::HttpConnection::DoGet("http://www.sylar.top/blog/", 300);
    AZURE_LOG_INFO(g_logger) << "result=" << rt2->result
                             << " error=" << rt2->error
                             << " rsp=" << (rt2->response ? rt2->response->toString() : "");
}

int main(int argc, char **argv) {
    azure::IOManager iom(2);
    iom.schedule(run);
    return 0;
}