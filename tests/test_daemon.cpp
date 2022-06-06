#include "daemon.h"
#include "iomanager.h"
#include "log.h"

azure::Logger::ptr g_logger = AZURE_LOG_ROOT();

azure::Timer::ptr timer;

int server_main(int argc, char **argv) {
    AZURE_LOG_INFO(g_logger) << azure::ProcessInfoMgr::GetInstance()->toString();
    azure::IOManager iom(1);
    timer = iom.addTimer(1000, [](){
        AZURE_LOG_INFO(g_logger) << "onTimer";
        static int count = 0;
        if(++count > 10) {
            timer->cancel();
        }
    }, true);
    return 0;
}

int main(int argc, char **argv) {
    return azure::start_daemon(argc, argv, server_main, argc != 1);
}