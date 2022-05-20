#include "http/http.h"

void test_request() {
    azure::http::HttpRequest::ptr req(new azure::http::HttpRequest);
    req->setHeader("host", "www.baidu.com");
    req->setBody("hello world");
    req->dump(std::cout) << std::endl;
}

void test_response() {
    azure::http::HttpResponse::ptr rsp(new azure::http::HttpResponse);
    rsp->setHeader("X-X", "azure");
    rsp->setBody("hello azure");
    rsp->setStatus(azure::http::HttpStatus(400));
    rsp->setClose(false);
    rsp->dump(std::cout) << std::endl;
}

int main(int argc, char **argv) {
    test_request();
    test_response();
    return 0;
}