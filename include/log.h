#ifndef __AZURE_LOG_H__
#define __AZURE_LOG_H__

#include <string>
#include <vector>
#include <cstdint>
#include <memory>
#include <list>
#include <sstream>
#include <fstream>
#include <map>
#include <functional>
#include <iostream>
#include <ctime>
#include <cstdarg>
#include "util.h"
#include "singleton.h"

// construct后立即析构，立刻得到输出
#define AZURE_LOG_LEVEL(logger, level) \
    if(logger->getLevel() <= level) \
        azure::LogEventWrap(azure::LogEvent::ptr(new azure::LogEvent(logger, level , \
                                                __FILE__, __LINE__, 0, azure::GetThreadId(), \
                                                azure::GetFiberId(), time(0)))).getSs()

#define AZURE_LOG_DEBUG(logger) AZURE_LOG_LEVEL(logger, azure::LogLevel::DEBUG)
#define AZURE_LOG_INFO(logger) AZURE_LOG_LEVEL(logger, azure::LogLevel::INFO)
#define AZURE_LOG_WARN(logger) AZURE_LOG_LEVEL(logger, azure::LogLevel::WARN)
#define AZURE_LOG_ERROR(logger) AZURE_LOG_LEVEL(logger, azure::LogLevel::ERROR)
#define AZURE_LOG_FATAL(logger) AZURE_LOG_LEVEL(logger, azure::LogLevel::FATAL)

#define AZURE_LOG_FMT_LEVEL(logger, level, fmt, ...) \
    if(logger->getLevel() <= level) \
        azure::LogEventWrap(azure::LogEvent::ptr(new azure::LogEvent(logger, level, \
                                                __FILE__, __LINE__, 0, azure::GetThreadId(), \
                                                azure::GetFiberId(), time(0)))). \
                                                getEvent()->format(fmt, __VA_ARGS__)

#define AZURE_LOG_FMT_DEBUG(logger, fmt, ...) AZURE_LOG_FMT_LEVEL(logger, azure::LogLevel::DEBUG, fmt, __VA_ARGS__)
#define AZURE_LOG_FMT_INFO(logger, fmt, ...) AZURE_LOG_FMT_LEVEL(logger, azure::LogLevel::INFO, fmt, __VA_ARGS__)
#define AZURE_LOG_FMT_WARN(logger, fmt, ...) AZURE_LOG_FMT_LEVEL(logger, azure::LogLevel::WARN, fmt, __VA_ARGS__)
#define AZURE_LOG_FMT_ERROR(logger, fmt, ...) AZURE_LOG_FMT_LEVEL(logger, azure::LogLevel::ERROR, fmt, __VA_ARGS__)
#define AZURE_LOG_FMT_FATAL(logger, fmt, ...) AZURE_LOG_FMT_LEVEL(logger, azure::LogLevel::FATAL, fmt, __VA_ARGS__)

#define AZURE_LOG_ROOT() azure::LoggerMgr::GetInstance()->getRoot()
#define AZURE_LOG_NAME(name) azure::LoggerMgr::GetInstance()->getLogger(name)

namespace azure {

class Logger;
class LoggerManager;

// 日志级别
class LogLevel {
public:
    enum Level {
        UNKNOWN = 0,
        DEBUG = 1,
        INFO = 2,
        WARN = 3,
        ERROR = 4,
        FATAL = 5
    };

    static const char *ToString(LogLevel::Level level);
    static LogLevel::Level FromString(const std::string str);
};

// 日志事件
class LogEvent {
public:
    typedef std::shared_ptr<LogEvent> ptr;
    LogEvent(std::shared_ptr<Logger> logger, LogLevel::Level level, const char *file, int32_t line, uint32_t elapse, uint32_t threadId, uint32_t fiberId, uint64_t time);
    ~LogEvent() {};

    const char *getFile() const {return m_file;}
    int32_t getLine() const {return m_line;}
    uint32_t getElapse() const {return m_elapse;}
    uint32_t getThreadId() const {return m_threadId;}
    uint32_t getFiberId() const {return m_fiberId;}
    uint64_t getTime() const {return m_time;}
    std::string getContent() const {return m_ss.str();}
    std::shared_ptr<Logger> getLogger() const {return m_logger;}
    LogLevel::Level getLevel() const {return m_level;}

    std::stringstream &getSs() {return m_ss;}
    void format(const char *fmt, ...);
    void format(const char *fmt, va_list al);

private:
    std::shared_ptr<Logger> m_logger;   // 处理该事件的Logger
    LogLevel::Level m_level;            // 日志级别
    const char *m_file = nullptr;       // 文件名
    int32_t m_line = 0;                 // 行号
    uint32_t m_elapse = 0;              // 程序启动开始到现在的毫秒数
    uint32_t m_threadId = 0;            // 线程ID
    uint32_t m_fiberId = 0;             // 协程ID
    uint64_t m_time;                    // 时间戳
    std::stringstream m_ss;             // 日志流
};

class LogEventWrap {
public:
    LogEventWrap(LogEvent::ptr e);
    ~LogEventWrap();
    LogEvent::ptr getEvent() const {return m_event;}
    std::stringstream &getSs();

private:
    LogEvent::ptr m_event;
};

// 日志格式器
class LogFormatter {
public:
    typedef std::shared_ptr<LogFormatter> ptr;
    // 
    LogFormatter(const std::string &pattern);
    // 将LogEvent格式化成字符串，提供给LogAppender
    std::string format(std::shared_ptr<Logger> logger, LogLevel::Level level, LogEvent::ptr event);

    const std::string getPattern() const {return m_pattern;}

public:
    class FormatItem {
        public:
            typedef std::shared_ptr<FormatItem> ptr;
            virtual ~FormatItem() {}    // LEARN 父虚析构函数
            virtual void format(std::ostream &os, std::shared_ptr<Logger> logger, LogLevel::Level level, LogEvent::ptr event) = 0; 
    };

    void init();

    bool isError() const {return m_error;}

private:
    std::string m_pattern;
    std::vector<FormatItem::ptr> m_items;
    bool m_error = false;
};

// 日志输出地
class LogAppender {
public:
    typedef std::shared_ptr<LogAppender> ptr;

    LogAppender() {m_level = LogLevel::DEBUG;}
    virtual ~LogAppender() {}

    virtual void log(std::shared_ptr<Logger> logger, LogLevel::Level level, LogEvent::ptr event) = 0;

    void setFormatter(LogFormatter::ptr val) {m_formatter = val;}
    LogFormatter::ptr getFormatter() const {return m_formatter;}
    void setLevel(LogLevel::Level level) {m_level = level;}
    LogLevel::Level getLevel() const {return m_level;}

    // 将LogAppender的配置转成字符串
    virtual std::string toYamlString() = 0;

protected:
    LogLevel::Level m_level;
    LogFormatter::ptr m_formatter;
};

// 日志器
class Logger : public std::enable_shared_from_this<Logger> {
friend class LoggerManager;
public:
    typedef std::shared_ptr<Logger> ptr;

    Logger(const std::string &name="root");

    void log(LogLevel::Level level, LogEvent::ptr event);

    // void debug(LogEvent::ptr event);
    // void info(LogEvent::ptr event);
    // void warn(LogEvent::ptr event);
    // void error(LogEvent::ptr event);
    // void fatal(LogEvent::ptr event);

    void addAppender(LogAppender::ptr appender);
    void delAppender(LogAppender::ptr appender);
    void clearAppender();
    LogLevel::Level getLevel() const {return m_level;}
    void setLevel(LogLevel::Level val) {m_level = val;}
    const std::string &getName() const {return m_name;}

    void setFormatter(LogFormatter::ptr val);
    void setFormatter(const std::string &val);
    LogFormatter::ptr getFormatter();

    // 将logger的配置转成字符串输出
    std::string toYamlString();

private:
    std::string m_name;                         // 日志名称
    LogLevel::Level m_level;                    // 日志级别
    std::list<LogAppender::ptr> m_appenders;    // Appender集合
    LogFormatter::ptr m_formatter;        
    Logger::ptr m_root;                         // 等同于LoggerManager的m_root，用来指示root logger
};

// 定义输出到文件的Appender
class FileLogAppender : public LogAppender {
public:
    typedef std::shared_ptr<FileLogAppender> ptr;
    FileLogAppender(const std::string &filename);
    void log(Logger::ptr logger, LogLevel::Level level, LogEvent::ptr event) override;
    bool reopen();  // 重新打开文件，打开成功返回true
    std::string toYamlString() override;

private:
    std::string m_filename;
    std::ofstream m_filestream;
};

// 输出到控制台的Appender
class StdoutLogAppender : public LogAppender {
public:
    typedef std::shared_ptr<StdoutLogAppender> ptr;
    StdoutLogAppender();
    void log(Logger::ptr logger, LogLevel::Level level, LogEvent::ptr event) override;
    std::string toYamlString() override;
};

class LoggerManager {
public:
    LoggerManager();
    Logger::ptr getLogger(const std::string &name);
    Logger::ptr getRoot() const {return m_root;}
    void init();

    std::string toYamlString();

private:
    std::map<std::string, Logger::ptr> m_loggers;
    Logger::ptr m_root;
};

typedef azure::Singleton<LoggerManager> LoggerMgr;

}

#endif