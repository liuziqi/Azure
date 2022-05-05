#include <iostream>
#include "../src/log.h"

int main(int argc, char **argv) {
    azure::Logger::ptr logger(new azure::Logger);
    
    logger->addAppender(azure::LogAppender::ptr(new azure::StdoutLogAppender));

    azure::FileLogAppender::ptr file_appender(new azure::FileLogAppender("/home/lzq/Azure/log/log.txt"));
    file_appender->setLevel(azure::LogLevel::ERROR);
    logger->addAppender(file_appender);

    // azure::LogEvent::ptr event(new azure::LogEvent(__FILE__, __LINE__, 0, azure::GetThreadId(), azure::GetFiberId(), time(0)));
    // logger->log(azure::LogLevel::DEBUG, event);

    AZURE_LOG_INFO(logger) << "test macro";
    AZURE_LOG_ERROR(logger) << "test macro error";

    AZURE_LOG_FMT_ERROR(logger, "test macro fmt error %s", "haha");

    // 单例模式管理logger
    auto l = azure::LoggerMgr::GetInstance()->getLogger("xx");
    AZURE_LOG_INFO(l) << "xxx";

    return 0;
}