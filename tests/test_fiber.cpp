#include "azure.h"

azure::Logger::ptr g_logger = AZURE_LOG_ROOT();

void run_in_fiber() {
    AZURE_LOG_INFO(g_logger) << "run_in_fiber begin";
    azure::Fiber::YieldToHold();
    AZURE_LOG_INFO(g_logger) << "run_in_fiber end";
    azure::Fiber::YieldToHold();
}

void test_fiber() {
    azure::Fiber::GetThis();    // 初始化主协程
    AZURE_LOG_INFO(g_logger) << "main begin";
    azure::Fiber::ptr fiber(new azure::Fiber(run_in_fiber));
    fiber->swapIn();
    AZURE_LOG_INFO(g_logger) << "main after swapin";
    fiber->swapIn();
    AZURE_LOG_INFO(g_logger) << "main end";
    fiber->swapIn();
    AZURE_LOG_INFO(g_logger) << "main finish";
}

int main(int argc, char **argv) {
    std::vector<azure::Thread::ptr> thrs;
    for(int i = 0; i < 3; ++i) {
        thrs.push_back(azure::Thread::ptr(new azure::Thread(&test_fiber, "name_" + std::to_string(i))));
    }
    for(auto &t : thrs) {
        t->join();
    }
    return 0;
}