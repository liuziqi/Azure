#include "hook.h"
#include "iomanager.h"
#include "log.h"

azure::Logger::ptr g_logger = AZURE_LOG_ROOT();

void test_sleep() {
    azure::IOManager iom(1);
    
    // NOTE 用hook把同步变成异步：sleep换成超时操作
    iom.schedule([](){
        sleep(2);
        AZURE_LOG_INFO(g_logger) << "sleep 2";
    });

    iom.schedule([](){
        sleep(3);
        AZURE_LOG_INFO(g_logger) << "sleep 3";
    });

    AZURE_LOG_INFO(g_logger) << "test_sleep";
}

int main(int argc, char **argv) {
    test_sleep();
    return 0;
}