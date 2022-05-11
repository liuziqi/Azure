#include "azure.h"

azure::Logger::ptr g_logger = AZURE_LOG_ROOT();

void tese_fiber() {
    AZURE_LOG_INFO(g_logger) << "test in fiber";
}

int main(int argc, char **argv) {
    AZURE_LOG_INFO(g_logger) << "main";
    azure::Scheduler sc;
    sc.schedule(&tese_fiber);
    sc.start();
    // AZURE_LOG_INFO(g_logger) << "schedule";
    sc.schedule(&tese_fiber);
    sc.stop();
    AZURE_LOG_INFO(g_logger) << "stop";
    return 0;
}