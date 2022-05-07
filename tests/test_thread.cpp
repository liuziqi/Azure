#include "azure.h"
#include "unistd.h"

azure::Logger::ptr g_logger = AZURE_LOG_ROOT();

// 防止编译器优化
// 不然编译器在编译的时候直接整一个 count+=100000
volatile int count = 0;

azure::Mutex s_mutex;

void func1() {
    AZURE_LOG_INFO(g_logger) << "name: " << azure::Thread::GetName() 
                                << " this.name: " << azure::Thread::GetThis()->getName()
                                << " id: " << azure::GetThreadId()
                                << " this.id: " << azure::Thread::GetThis()->getId();
    // sleep(20);
    for(int i = 0; i < 100000; ++i) {
        azure::Mutex::Lock lock(s_mutex);
        ++count;
    }
}

void func2() {
    while(true) {
        AZURE_LOG_INFO(g_logger) << "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx";
    }
}

void func3() {
    while(true) {
        AZURE_LOG_INFO(g_logger) << "============================================";
    }
}

int main(int argc, char **argv) {
    AZURE_LOG_INFO(g_logger) << "thread test begin";

    YAML::Node root = YAML::LoadFile("/home/lzq/Azure/cfg/log_cfg.yml");
    azure::Config::LoadFromYaml(root);

    std::vector<azure::Thread::ptr> thrs;
    for(int i = 0; i < 2; ++i) {
        azure::Thread::ptr thr1(new azure::Thread(&func2, "name_" + std::to_string(i * 2)));
        azure::Thread::ptr thr2(new azure::Thread(&func3, "name_" + std::to_string(i * 2 + 1)));
        thrs.push_back(thr1);
        thrs.push_back(thr2);
    }

    // 线程在创建的时候就已经开始执行了，这里开始阻塞
    for(size_t i = 0; i < thrs.size(); ++i) {
        thrs[i]->join();    // 阻塞调用它的线程，直至目标线程执行结束
    }
    
    AZURE_LOG_INFO(g_logger) << "thread test end";
    return 0;
}