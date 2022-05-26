#include <iostream>
#include "uri.h"

int main(int argc, char **argv) {
    // azure::Uri::ptr uri = azure::Uri::Create("http://www.sylar.top/test/uri?id=100&name=sylar#frag");
    azure::Uri::ptr uri = azure::Uri::Create("http://admin@www.sylar.top/test/中文/uri?id=100&name=sylar&vv=中文#frag中文");
    std::cout << uri->toString() << std::endl;
    auto addr = uri->createAddress();
    std::cout << *addr << std::endl;
    return 0;
}
