#include "azure.h"

azure::Logger::ptr g_logger = AZURE_LOG_ROOT();

void test_fiber() {
    static int s_count = 5;
    AZURE_LOG_INFO(g_logger) << "test in fiber s_count=" << s_count;
    sleep(1);
    if(--s_count >= 0) {
        // azure::Scheduler::GetThis()->schedule(&test_fiber, azure::GetThreadId());
        azure::Scheduler::GetThis()->schedule(&test_fiber);
    }
}

int main(int argc, char **argv) {
    azure::Thread::SetName("mainth");
    AZURE_LOG_INFO(g_logger) << "main";
    azure::Scheduler sc(1, true, "test");
    // sc.schedule(&tese_fiber);
    sc.start();
    AZURE_LOG_INFO(g_logger) << "schedule";
    sleep(2);
    sc.schedule(&test_fiber);
    sc.stop();
    AZURE_LOG_INFO(g_logger) << "stop";
    return 0;
}