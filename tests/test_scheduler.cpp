#include "azure.h"

azure::Logger::ptr g_logger = AZURE_LOG_ROOT();

void test_fiber() {
    static int s_count = 5;
    AZURE_LOG_INFO(g_logger) << "test in fiber s_count=" << s_count;
    sleep(1);
    if(--s_count >= 0) {
        // azure::Scheduler::GetThis()->schedule(&test_fiber, azure::GetThreadId());
        azure::Scheduler::GetThis()->schedule(&test_fiber, azure::GetThreadId());
    }
}

int main(int argc, char **argv) {
    AZURE_LOG_INFO(g_logger) << "main";
    azure::Scheduler sc(3, false, "test");
    // sc.schedule(&tese_fiber);
    sc.start();
    AZURE_LOG_INFO(g_logger) << "schedule";
    sc.schedule(&test_fiber);
    sc.stop();
    AZURE_LOG_INFO(g_logger) << "stop";
    return 0;
}