/**
 * @file log.h
 * @brief 封装日志模块
 * @author liuziqi
 * @date 2022-05-30
 * @version 0.1
 * @copyright Copyright (c) 2022
 */
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
#include "log.h"
#include "thread.h"
#include "mutex.h"

/**
 * @brief 使用流式方式将日志级别level的日志写入到logger
 * @details 使用LogEventWrap管理资源，在LogEventWrap匿名对象的析构函数里进行日志输出
 */
#define AZURE_LOG_LEVEL(logger, level) \
    if(logger->getLevel() <= level) \
        azure::LogEventWrap(azure::LogEvent::ptr(new azure::LogEvent(logger, level , \
                                                __FILE__, __LINE__, 0, azure::GetThreadId(), \
                                                azure::GetFiberId(), time(0), azure::Thread::GetName()))).getSs()

/**
 * @brief 使用流式方式将debug日志级别的日志写入到logger
 */
#define AZURE_LOG_DEBUG(logger) AZURE_LOG_LEVEL(logger, azure::LogLevel::DEBUG)

/**
 * @brief 使用流式方式将info日志级别的日志写入到logger
 */
#define AZURE_LOG_INFO(logger) AZURE_LOG_LEVEL(logger, azure::LogLevel::INFO)

/**
 * @brief 使用流式方式将warn日志级别的日志写入到logger
 */
#define AZURE_LOG_WARN(logger) AZURE_LOG_LEVEL(logger, azure::LogLevel::WARN)

/**
 * @brief 使用流式方式将日志级别info的日志写入到logger
 */
#define AZURE_LOG_ERROR(logger) AZURE_LOG_LEVEL(logger, azure::LogLevel::ERROR)

/**
 * @brief 使用流式方式将日志级别fatal的日志写入到logger
 */
#define AZURE_LOG_FATAL(logger) AZURE_LOG_LEVEL(logger, azure::LogLevel::FATAL)

/**
 * @brief 使用格式化方式将level日志级别的日志写入到logger
 */
#define AZURE_LOG_FMT_LEVEL(logger, level, fmt, ...) \
    if(logger->getLevel() <= level) \
        azure::LogEventWrap(azure::LogEvent::ptr(new azure::LogEvent(logger, level, \
                                                __FILE__, __LINE__, 0, azure::GetThreadId(), \
                                                azure::GetFiberId(), time(0), azure::Thread::GetName()))). \
                                                getEvent()->format(fmt, __VA_ARGS__)
/**
 * @brief 使用格式化方式将debug日志级别的日志写入到logger
 */
#define AZURE_LOG_FMT_DEBUG(logger, fmt, ...) AZURE_LOG_FMT_LEVEL(logger, azure::LogLevel::DEBUG, fmt, __VA_ARGS__)

/**
 * @brief 使用格式化方式将info日志级别的日志写入到logger
 */
#define AZURE_LOG_FMT_INFO(logger, fmt, ...) AZURE_LOG_FMT_LEVEL(logger, azure::LogLevel::INFO, fmt, __VA_ARGS__)

/**
 * @brief 使用格式化方式将warn日志级别的日志写入到logger
 */
#define AZURE_LOG_FMT_WARN(logger, fmt, ...) AZURE_LOG_FMT_LEVEL(logger, azure::LogLevel::WARN, fmt, __VA_ARGS__)

/**
 * @brief 使用格式化方式将error日志级别的日志写入到logger
 */
#define AZURE_LOG_FMT_ERROR(logger, fmt, ...) AZURE_LOG_FMT_LEVEL(logger, azure::LogLevel::ERROR, fmt, __VA_ARGS__)

/**
 * @brief 使用格式化方式将fatal日志级别的日志写入到logger
 */
#define AZURE_LOG_FMT_FATAL(logger, fmt, ...) AZURE_LOG_FMT_LEVEL(logger, azure::LogLevel::FATAL, fmt, __VA_ARGS__)


/**
 * @brief 获取主日志器
 */
#define AZURE_LOG_ROOT() azure::LoggerMgr::GetInstance()->getRoot()

/**
 * @brief 获取名称为name的日志器
 */
#define AZURE_LOG_NAME(name) azure::LoggerMgr::GetInstance()->getLogger(name)

namespace azure {

class Logger;
class LoggerManager;

/**
 * @brief 日志级别
 */
class LogLevel {
public:
    /**
     * @brief 日志级别的枚举值
     */
    enum Level {
        /// 未知级别
        UNKNOWN = 0,
        /// DEBUG 级别
        DEBUG = 1,
        /// INFO 级别
        INFO = 2,
        /// WARN 级别
        WARN = 3,
        /// ERROR 级别
        ERROR = 4,
        /// FATAL 级别
        FATAL = 5
    };

    /**
     * @brief 将日志级别转成文本输出
     * @param level 日志级别
     * @return const char* 日志级别对应的文本
     */
    static const char *ToString(LogLevel::Level level);

    /**
     * @brief 将文本转换成日志级别
     * @param str 日志级别文本
     * @return LogLevel::Level 日志级别
     */
    static LogLevel::Level FromString(const std::string &str);
};

/**
 * @brief 日志事件
 */
class LogEvent {
public:
    typedef std::shared_ptr<LogEvent> ptr;

    /**
     * @brief 构造函数
     * @param logger 日志器
     * @param level 日志级别
     * @param file 文件名称
     * @param line 当前所在代码行号
     * @param elapse 程序启动依赖的耗时（毫秒）
     * @param threadId 线程id
     * @param fiberId 协程id
     * @param time 输出当前时间（秒）
     * @param thread_name 线程名称
     */
    LogEvent(std::shared_ptr<Logger> logger, LogLevel::Level level, const char *file, int32_t line, uint32_t elapse, 
                uint32_t threadId, uint32_t fiberId, uint64_t time, const std::string &thread_name);
    
    /**
     * @brief 析构函数，函数体为空
     */
    ~LogEvent() {};

    /**
     * @brief 获取文件名称
     */
    const char *getFile() const {return m_file;}

    /**
     * @brief 获取代码行号
     */
    int32_t getLine() const {return m_line;}

    /**
     * @brief 获取程序启动依赖的耗时（毫秒）
     */
    uint32_t getElapse() const {return m_elapse;}

    /**
     * @brief 获取线程id
     */
    uint32_t getThreadId() const {return m_threadId;}

    /**
     * @brief 获取线程名称
     */
    std::string getThreadName() const {return m_threadName;}

    /**
     * @brief 获取协程名称
     */    
    uint32_t getFiberId() const {return m_fiberId;}

    /**
     * @brief 获取当前时间（秒）
     */  
    uint64_t getTime() const {return m_time;}

    /**
     * @brief 获取日志内容
     */ 
    std::string getContent() const {return m_ss.str();}

    /**
     * @brief 获取日志器
     */ 
    std::shared_ptr<Logger> getLogger() const {return m_logger;}

    /**
     * @brief 获取日志级别
     */ 
    LogLevel::Level getLevel() const {return m_level;}

    /**
     * @brief 获取日志内容字符串流
     */ 
    std::stringstream &getSs() {return m_ss;}

    /**
     * @brief 格式化写入日志内容
     */
    void format(const char *fmt, ...);

    /**
     * @brief 格式化写入日志内容
     */
    void format(const char *fmt, va_list al);

private:
    /// 处理该事件的Logger
    std::shared_ptr<Logger> m_logger;
    /// 日志级别
    LogLevel::Level m_level;
    /// 文件名           
    const char *m_file = nullptr;
    /// 行号      
    int32_t m_line = 0;
    /// 程序启动开始到现在的毫秒数                
    uint32_t m_elapse = 0;
    /// 线程ID              
    uint32_t m_threadId = 0;
    /// 线程名称            
    std::string m_threadName;
    /// 协程ID         
    uint32_t m_fiberId = 0;
    /// 时间戳            
    uint64_t m_time;
    /// 日志流                  
    std::stringstream m_ss;             
};

/**
 * @brief 日志事件包装器
 */
class LogEventWrap {
public:
    /**
     * @brief 构造函数
     * @param e 日志事件
     */
    LogEventWrap(LogEvent::ptr e);

    /**
     * @brief 析构函数，在析构函数里输出日志内容
     */
    ~LogEventWrap();

    /**
     * @brief 获取日志事件
     */
    LogEvent::ptr getEvent() const {return m_event;}

    /**
     * @brief 获取日志内容字符串流
     */
    std::stringstream &getSs();

private:
    LogEvent::ptr m_event;  
};

/**
 * @brief 日志格式器
 */
class LogFormatter {
public:
    typedef std::shared_ptr<LogFormatter> ptr;

    /**
     * @brief 构造函数
     * @param pattern 格式模板
     * @details 
     *  %m 消息
     *  %p 日志级别
     *  %r 累计毫秒数
     *  %c 日志名称
     *  %t 线程id
     *  %n 换行
     *  %d 时间
     *  %f 文件名
     *  %l 行号
     *  %T 制表符
     *  %F 协程id
     *  %N 线程名称
     *  默认格式 "%d{%Y-%m-%d %H:%M:%S}%T%t%T%N%T%F%T[%p]%T[%c]%T%f:%l%T%m%n"
     */
    LogFormatter(const std::string &pattern);
    
    /**
     * @brief 将日志文本格式化
     * @param logger 日志器
     * @param level 日志级别
     * @param event 日志事件
     * @return std::string 格式化日志文本
     */
    std::string format(std::shared_ptr<Logger> logger, LogLevel::Level level, LogEvent::ptr event);

    /**
     * @brief 返回日志模板
     */
    const std::string getPattern() const {return m_pattern;}

    /**
     * @brief 是否解析日志出错
     */
    bool isError() const {return m_error;}

public:
    /**
     * @brief 日志内容项格式化
     */
    class FormatItem {
        public:
            typedef std::shared_ptr<FormatItem> ptr;

            /**
             * @brief 析构函数，函数体为空
             */
            virtual ~FormatItem() {}

            /**
             * @brief 格式化日志到流
             * @param os 日志输出流
             * @param logger 日志器
             * @param level 日志等级
             * @param event 日志事件
             */
            virtual void format(std::ostream &os, std::shared_ptr<Logger> logger, LogLevel::Level level, LogEvent::ptr event) = 0; 
    };

private:
    /**
     * @brief 初始化,解析日志模板
     */
    void init();

private:
    /// 日志格式模板
    std::string m_pattern;
    /// 日志格式解析后格式
    std::vector<FormatItem::ptr> m_items;
     /// 是否解析出错
    bool m_error = false;
};

/**
 * @brief 日志输出目标
 * @note 这里level相关的接口有冗余
 */
class LogAppender {
friend class Logger;

public:
    typedef std::shared_ptr<LogAppender> ptr;
    typedef Spinlock MutexType;

    /**
     * @brief 构造函数，为空
     */
    LogAppender() {}

    /**
     * @brief 析构函数，为空
     */
    virtual ~LogAppender() {}

    /**
     * @brief 根据LogAppender对象的formatter写入日志
     * @param logger 日志器
     * @param level 日志级别
     * @param event 日志事件
     */
    virtual void log(std::shared_ptr<Logger> logger, LogLevel::Level level, LogEvent::ptr event) = 0;

    /**
     * @brief 更改日志格式器
     */
    void setFormatter(LogFormatter::ptr val);

    /**
     * @brief 获取日志格式器
     */
    LogFormatter::ptr getFormatter();

    /**
     * @brief 设置日志级别
     */
    void setLevel(LogLevel::Level level) {m_level = level;}

    /**
     * @brief 获取日志级别
     */
    LogLevel::Level getLevel() const {return m_level;}

    /**
     * @brief 将LogAppender的配置转成YAML String
     */
    virtual std::string toYamlString() = 0;

protected:
    // NOTE 默认值自动继承
    /// 是否有自己的日志格式器
    bool m_hasFormatter = false;
    /// 日志级别
    LogLevel::Level m_level = LogLevel::DEBUG;
    /// 日志格式器
    LogFormatter::ptr m_formatter;
    /// 互斥更改m_formatter，因为m_formatter包含多个成员，对m_formatter的读写不是原子操作
    MutexType m_mutex;
};

/**
 * @brief 日志器
 */
class Logger : public std::enable_shared_from_this<Logger> {
friend class LoggerManager;

public:
    typedef std::shared_ptr<Logger> ptr;
    typedef Spinlock MutexType;

    /**
     * @brief 构造函数
     * @param name 日志器名称
     */
    Logger(const std::string &name="root");

    /**
     * @brief 写日志
     * @param level 日志级别
     * @param event 日志事件
     */
    void log(LogLevel::Level level, LogEvent::ptr event);

    // void debug(LogEvent::ptr event);
    // void info(LogEvent::ptr event);
    // void warn(LogEvent::ptr event);
    // void error(LogEvent::ptr event);
    // void fatal(LogEvent::ptr event);

    /**
     * @brief 添加日志输出目标
     * @param appender 日志目标
     */
    void addAppender(LogAppender::ptr appender);

    /**
     * @brief 删除日志目标
     * @param appender 日志目标
     */
    void delAppender(LogAppender::ptr appender);

    /**
     * @brief 清空日志目标
     */
    void clearAppender();

    /**
     * @brief 获取日志级别
     */
    LogLevel::Level getLevel() const {return m_level;}

    /**
     * @brief 设置日志级别
     */
    void setLevel(LogLevel::Level val) {m_level = val;}

    /**
     * @brief 返回日志器名称
     */
    const std::string &getName() const {return m_name;}

    /**
     * @brief 设置默认日志格式器
     */
    void setFormatter(LogFormatter::ptr val);

    /**
     * @brief 设置默认日志格式器
     */
    void setFormatter(const std::string &val);

    /**
     * @brief 获取默认日志格式器
     */
    LogFormatter::ptr getFormatter();

    /**
     * @brief 将日志器的配置转成YAML String
     */
    std::string toYamlString();

private:
    /// 日志名称
    std::string m_name;
    /// 日志级别
    LogLevel::Level m_level;
    /// 日志目标集合
    std::list<LogAppender::ptr> m_appenders;
    /// 日志格式器
    LogFormatter::ptr m_formatter;
    /// 等同于LoggerManager的m_root，用来指示root logger
    Logger::ptr m_root;
    /// 用于互斥访问m_appenders
    MutexType m_mutex;
};

/**
 * @brief 输出到文件的Appender
 */
class FileLogAppender : public LogAppender {
public:
    typedef std::shared_ptr<FileLogAppender> ptr;

    /**
     * @brief 构造函数
     * @param filename 目标文件路径
     */
    FileLogAppender(const std::string &filename);

    /**
     * @brief 根据LogAppender对象的formatter写入日志
     * @param logger 日志器
     * @param level 日志级别
     * @param event 日志事件
     */
    void log(Logger::ptr logger, LogLevel::Level level, LogEvent::ptr event) override;

    /**
     * @brief 重新打开日志文件
     * @return bool 成功返回true，失败返回false
     */
    bool reopen();

    /**
     * @brief 将FileLogAppender的配置转成YAML String
     */
    std::string toYamlString() override;

private:
    /// 文件路径
    std::string m_filename;
    /// 文件流
    std::ofstream m_filestream;
};

/**
 * @brief 输出到控制台的Appender
 */
class StdoutLogAppender : public LogAppender {
public:
    typedef std::shared_ptr<StdoutLogAppender> ptr;

    /**
     * @brief 构造函数
     */
    StdoutLogAppender();

    /**
     * @brief 根据LogAppender对象的formatter写入日志
     * @param logger 日志器
     * @param level 日志级别
     * @param event 日志事件
     */
    void log(Logger::ptr logger, LogLevel::Level level, LogEvent::ptr event) override;

    /**
     * @brief 将StdoutLogAppender的配置转成YAML String
     */
    std::string toYamlString() override;
};

/**
 * @brief 日志器管理类
 */
class LoggerManager {
public:
    typedef Spinlock MutexType;

    /**
     * @brief 构造函数
     */
    LoggerManager();

    /**
     * @brief 获取日志器
     * @param name 日志器名称
     */
    Logger::ptr getLogger(const std::string &name);

    /**
     * @brief 返回主日志器
     */
    Logger::ptr getRoot() const {return m_root;}

    /**
     * @brief 将所有的日志器配置转成YAML String
     */
    std::string toYamlString();

private:
    /**
     * @brief 初始化
     */
    void init();

private:
    /// Mutex，互斥访问m_loggers
    MutexType m_mutex;
    /// 日志器容器
    std::map<std::string, Logger::ptr> m_loggers;
    /// 主日志器
    Logger::ptr m_root;
};

/// 日志器管理类单例模式
typedef azure::Singleton<LoggerManager> LoggerMgr;

}

#endif