#include <iostream>
#include <fstream>
#include "env.h"

struct A {
    A() {
        std::ifstream ifs("/proc/" + std::to_string(getpid()) + "/cmdline", std::ios::binary);
        std::string content;
        content.resize(4096);

        ifs.read(&content[0], content.size());
        content.resize(ifs.gcount());

        for(size_t i = 0; i < content.size(); ++i) {
            std::cout << i << " - " << content[i] << " - " << (int)content[i] << std::endl;
        }

        std::cout << content << std::endl;
    }
};

A a;

int main(int argc, char **argv) {
    azure::EnvMgr::GetInstance()->addHelp("s", "start with the terminal");
    azure::EnvMgr::GetInstance()->addHelp("d", "run as daemon");
    azure::EnvMgr::GetInstance()->addHelp("p", "print help");
    if(!azure::EnvMgr::GetInstance()->init(argc, argv)) {
        azure::EnvMgr::GetInstance()->printHelp();
        return 0;
    }

    std::cout << "exe=" << azure::EnvMgr::GetInstance()->getExe() << std::endl;
    std::cout << "cwd=" << azure::EnvMgr::GetInstance()->getCwd() << std::endl;

    // std::cout << "path=" << azure::EnvMgr::GetInstance()->getEnv("PATH", "xxx") << std::endl;
    std::cout << "test=" << azure::EnvMgr::GetInstance()->getEnv("TEST", "xxx") << std::endl;

    std::cout << "set env " << azure::EnvMgr::GetInstance()->setEnv("TEST", "yy") << std::endl;
    std::cout << "test=" << azure::EnvMgr::GetInstance()->getEnv("TEST", "") << std::endl;

    while(true) {
        std::cout << "test_aa=" << azure::EnvMgr::GetInstance()->getEnv("TEST_AA", "") << std::endl;
        sleep(3);
    }

    if(azure::EnvMgr::GetInstance()->has("p")) {
        azure::EnvMgr::GetInstance()->printHelp();
    }
    return 0;
}