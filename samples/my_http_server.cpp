#include "http/http_server.h"
#include "log.h"

azure::Logger::ptr g_logger = AZURE_LOG_ROOT();

void run() {
    g_logger->setLevel(azure::LogLevel::INFO);

    azure::Address::ptr addr = azure::Address::LookupAnyIPAddress("0.0.0.0:8020");
    if(!addr) {
        AZURE_LOG_ERROR(g_logger) << "get address error";
        return;
    }

    // azure::http::HttpServer::ptr http_server(new azure::http::HttpServer);
    azure::http::HttpServer::ptr http_server(new azure::http::HttpServer(true));    // 长连接

    auto sd = http_server->getServletDispatch();
    sd->addServlet("/azure/xx", [](azure::http::HttpRequest::ptr req, azure::http::HttpResponse::ptr rsp, azure::http::HttpSession::ptr session) {
        rsp->setBody("hello");
        return 0;
    });

    while(!http_server->bind(addr)) {
        AZURE_LOG_ERROR(g_logger) << "bind " << addr << " fail";
        sleep(1);
    }

    http_server->start();
}

int main(int argc, char **argv) {
    azure::IOManager iom(3);
    iom.schedule(run);
    return 0;
}