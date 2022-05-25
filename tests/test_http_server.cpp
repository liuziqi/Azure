#include "http/http_server.h"
#include "iomanager.h"
#include "log.h"

static azure::Logger::ptr g_logger = AZURE_LOG_ROOT();

void run() {
    azure::http::HttpServer::ptr server(new azure::http::HttpServer);
    azure::Address::ptr addr = azure::Address::LookupAnyIPAddress("0.0.0.0:8020");
    while(!server->bind(addr)) {
        sleep(2);
    }
    auto sd = server->getServletDispatch();
    sd->addServlet("/azure/xx", [](azure::http::HttpRequest::ptr req, azure::http::HttpResponse::ptr rsp, azure::http::HttpSession::ptr session) {
        rsp->setBody(req->toString());
        return 0;
    });
    sd->addGlobServlet("/azure/*", [](azure::http::HttpRequest::ptr req, azure::http::HttpResponse::ptr rsp, azure::http::HttpSession::ptr session) {
        rsp->setBody("Glob:\r\n" + req->toString());
        return 0;
    });
    server->start();
}

int main(int argc, char **argv) {
    azure::IOManager iom(2);
    iom.schedule(run);
    return 0;
}