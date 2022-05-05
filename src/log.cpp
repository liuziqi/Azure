#include "log.h"

namespace azure {

const char *LogLevel::ToString(LogLevel::Level level) {
    switch(level) {

#define XX(name) \
    case LogLevel::name: \
        return #name; \
        break;
    
    XX(DEBUG);
    XX(INFO);
    XX(WARN);
    XX(ERROR);
    XX(FATAL);
#undef XX

    default:
        return "UNKNOWN";
        break;
    }
    return "UNKNOWN";
}

LogEventWrap::LogEventWrap(LogEvent::ptr e)
    : m_event(e) {
}

LogEventWrap::~LogEventWrap() {
    // 对象离开作用域时自动写日志记录
    // 析构自动释放成员变量m_event，不需要手动管理
    m_event->getLogger()->log(m_event->getLevel(), m_event);
}

std::stringstream &LogEventWrap::getSs() {
    return m_event->getSs();
}

void LogEvent::format(const char *fmt, ...) {
    va_list al;
    va_start(al, fmt);  // 使al指向可变参数列表的第一个参数（从右向左入栈，所以最右边是第一个参数？）
    format(fmt, al);
    va_end(al);         // 将al置位nullptr
}

// LEARN 可变参数列表
void LogEvent::format(const char *fmt, va_list al) {
    char *buf = nullptr;
    int len = vasprintf(&buf, fmt, al);
    if(len != -1) {
        m_ss << std::string(buf, len);
        free(buf);
    }
}

class MessageFormatItem : public LogFormatter::FormatItem {
public:
    MessageFormatItem(const std::string &str="") {}

    void format(std::ostream &os, Logger::ptr logger, LogLevel::Level level, LogEvent::ptr event) override {
        os << event->getContent();
    }
};

class LevelFormatItem : public LogFormatter::FormatItem {
public:
    LevelFormatItem(const std::string &str="") {}

    void format(std::ostream &os, Logger::ptr logger, LogLevel::Level level, LogEvent::ptr event) override {
        os << LogLevel::ToString(level);
    }
};

class ElapseFormatItem : public LogFormatter::FormatItem {
public:
    ElapseFormatItem(const std::string &str="") {}

    void format(std::ostream &os, Logger::ptr logger, LogLevel::Level level, LogEvent::ptr event) override {
        os << event->getElapse();
    }
};

class NameFormatItem : public LogFormatter::FormatItem {
public:
    NameFormatItem(const std::string &str="") {}

    void format(std::ostream &os, Logger::ptr logger, LogLevel::Level level, LogEvent::ptr event) override {
        os << event->getLogger()->getName();
    }
};

class ThreadIdFormatItem : public LogFormatter::FormatItem {
public:
    ThreadIdFormatItem(const std::string &str="") {}

    void format(std::ostream &os, Logger::ptr logger, LogLevel::Level level, LogEvent::ptr event) override {
        os << event->getThreadId();
    }
};

class FiberIdFormatItem : public LogFormatter::FormatItem {
public:
    FiberIdFormatItem(const std::string &str="") {}

    void format(std::ostream &os, Logger::ptr logger, LogLevel::Level level, LogEvent::ptr event) override {
        os << event->getFiberId();
    }
};

class DateTimeFormatItem : public LogFormatter::FormatItem {
public:
    DateTimeFormatItem(const std::string &format="")
        : m_format(format) {
        if(m_format.empty()) {
            m_format = "%Y-%m-%d %H:%M:%S";
        }
    }

    void format(std::ostream &os, Logger::ptr logger, LogLevel::Level level, LogEvent::ptr event) override {
        struct tm tm;
        time_t time = event->getTime();
        localtime_r(&time, &tm);
        char buf[64];
        strftime(buf, sizeof(buf), m_format.c_str(), &tm);
        os << buf;
    }

private:
    std::string m_format;
};

class FilenameFormatItem : public LogFormatter::FormatItem {
public:
    FilenameFormatItem(const std::string &str="") {}

    void format(std::ostream &os, Logger::ptr logger, LogLevel::Level level, LogEvent::ptr event) override {
        os << event->getFile();
    }
};

class LineFormatItem : public LogFormatter::FormatItem {
public:
    LineFormatItem(const std::string &str="") {}

    void format(std::ostream &os, Logger::ptr logger, LogLevel::Level level, LogEvent::ptr event) override {
        os << event->getLine();
    }
};

class NewLineFormatItem : public LogFormatter::FormatItem {
public:
    NewLineFormatItem(const std::string &str="") {}

    void format(std::ostream &os, Logger::ptr logger, LogLevel::Level level, LogEvent::ptr event) override {
        os << "\n";
    }
};

class TabFormatItem : public LogFormatter::FormatItem {
public:
    TabFormatItem(const std::string &str="") {}

    void format(std::ostream &os, Logger::ptr logger, LogLevel::Level level, LogEvent::ptr event) override {
        os << "\t";
    }
};

class StringFormatItem : public LogFormatter::FormatItem {
public:
    StringFormatItem(const std::string &str)
        : m_string(str) {
    }

    void format(std::ostream &os, Logger::ptr logger, LogLevel::Level level, LogEvent::ptr event) override {
        os << m_string;
    }

private:
    std::string m_string;
};

LogEvent::LogEvent(std::shared_ptr<Logger> logger, LogLevel::Level level, const char *file, int32_t line, uint32_t elapse, uint32_t threadId, uint32_t fiberId, uint64_t time)
    : m_logger(logger)
    , m_level(level)
    , m_file(file)
    , m_line(line)
    , m_elapse(elapse)
    , m_threadId(threadId)
    , m_fiberId(fiberId)
    , m_time(time) {
}

Logger::Logger(const std::string &name) 
    : m_name(name)
    , m_level(LogLevel::DEBUG) {
    // m_formatter.reset(new LogFormatter("%d %t %F [%p] [%c] %f:%l %m%n"));
    m_formatter.reset(new LogFormatter("%d%T%r%T%t%T%F%T[%p]%T[%c]%T%f:%l%T%m%n"));
}

void Logger::addAppender(LogAppender::ptr appender) {
    if(!appender->getFormatter()) {
        appender->setFormatter(m_formatter);
    }
    // FIXME 先不考虑线程安全
    m_appenders.push_back(appender);
}

void Logger::delAppender(LogAppender::ptr appender) {
    for(auto it = m_appenders.begin(); it != m_appenders.end(); it++) {
        if(*it == appender) {
            m_appenders.erase(it);
            break;
        }
    }
}

void Logger::log(LogLevel::Level level, LogEvent::ptr event) {
    if(level >= m_level) {
        auto self = shared_from_this();
        // m_appenders不为空，用该logger输出
        if(!m_appenders.empty()) {
            for(auto &a : m_appenders) {
                a->log(self, level, event);
            }
        }
        // 否则用root logger输出
        else if(m_root) {
            m_root->log(level, event);
        }

    }
}

// void Logger::debug(LogEvent::ptr event) {
//     log(LogLevel::DEBUG, event);
// }

// void Logger::info(LogEvent::ptr event) {
//     log(LogLevel::INFO, event);
// }

// void Logger::warn(LogEvent::ptr event) {
//     log(LogLevel::WARN, event);
// }

// void Logger::error(LogEvent::ptr event) {
//     log(LogLevel::ERROR, event);
// }

// void Logger::fatal(LogEvent::ptr event) {
//     log(LogLevel::FATAL, event);
// }

FileLogAppender::FileLogAppender(const std::string &filename) 
    : LogAppender()
    , m_filename(filename) {
    reopen();
}

void FileLogAppender::log(Logger::ptr logger, LogLevel::Level level, LogEvent::ptr event) {
    if(level >= m_level) {
        m_filestream << m_formatter->format(logger, level, event);
    }
}

bool FileLogAppender::reopen() {
    if(m_filestream) {
        m_filestream.close();
    }
    m_filestream.open(m_filename);
    return !!m_filestream;  // 转换成真正的bool值
}

StdoutLogAppender::StdoutLogAppender()
    : LogAppender() {
}

void StdoutLogAppender::log(Logger::ptr logger, LogLevel::Level level, LogEvent::ptr event) {
    if(level >= m_level) {
        std::cout << m_formatter->format(logger, level, event);
    }
}

LogFormatter::LogFormatter(const std::string &pattern) 
    : m_pattern(pattern) {
    init();
}

std::string LogFormatter::format(Logger::ptr logger, LogLevel::Level level, LogEvent::ptr event) {
    std::stringstream ss;
    for(auto &i : m_items) {
        i->format(ss, logger, level, event);
    }
    return ss.str();
}

/*  
 * 待解析的格式类似于 "%x %x{xxx} %x %%"
 * 0 -- 直接输出
 * 1 -- 替换成对应的字符串
 * 2 -- 错误格式
 * %m -- 消息体
 * %t -- 日志级别
 * %r -- 自启动后到输出该信息的时间
 * %c -- 日志名称
 * %t -- 线程id
 * %n -- 回车换行
 * %d -- 时间
 * %f -- 文件名
 * %l -- 行号
 * %T -- 制表符
 * %F -- 协程id
 */
void LogFormatter::init() {
    static std::map<std::string, std::function<FormatItem::ptr(const std::string &str)>> s_format_items = {
#define XX(str, C) \
        {#str, [](const std::string &fmt) {return FormatItem::ptr(new C(fmt));}}
        XX(m, MessageFormatItem),
        XX(p, LevelFormatItem),
        XX(r, ElapseFormatItem),
        XX(c, NameFormatItem),
        XX(t, ThreadIdFormatItem),
        XX(n, NewLineFormatItem),
        XX(d, DateTimeFormatItem),
        XX(f, FilenameFormatItem),
        XX(l, LineFormatItem),
        XX(T, TabFormatItem),
        XX(F, FiberIdFormatItem)
#undef XX
    };

    std::vector<std::tuple<std::string, std::string, int>> vec;
    std::ostringstream ss;
    std::string fmt = "";

    for(size_t i = 0; i < m_pattern.size(); ++i) {
        if(m_pattern[i] != '%') {
            if(fmt == "") {
                ss << m_pattern[i];
                continue;
            }
            else {
                if(m_pattern[i] != '{') {
                    vec.push_back({fmt, "", 1});
                    fmt = "";
                    ss << m_pattern[i];
                    continue;
                }
                else {
                    size_t start = i + 1;
                    while(i < m_pattern.size() && m_pattern[i] != '}') ++i;
                    // 没有 '}'，错误格式
                    if(i == m_pattern.size()) {
                        vec.push_back({"lack of '}'", "", 2});
                    }
                    else {
                        vec.push_back({fmt, m_pattern.substr(start, i - start), 1});
                        fmt = "";
                        continue;
                    }
                }
            }
        }

        // m_pattern[i] == '%' 的情况
        if((i + 1) < m_pattern.size()) {
            // "%%"表示转义一个'%'
            if(m_pattern[i + 1] == '%') {
                ss << '%';
                ++i;
                continue;
            }
            else {
                if(fmt != "") {
                    vec.push_back({fmt, "", 1});
                    fmt = "";
                    i -= 1;
                }
                else {
                    // 得到了一个可以直接输出的字符串
                    std::string str = ss.str();
                    if(!str.empty()) {
                        vec.push_back({str, "", 0});
                        ss.str("");
                    }
                    // 得到格式
                    fmt = m_pattern[++i];
                }
            }
        }
        // 单个'%'后面没有字符了，错误格式
        else {
            vec.push_back({"single '%'", "", 2});
        }
    }

    std::string str = ss.str();
    if(!str.empty()) {
        vec.push_back({str, "", 0});
    }

    if(fmt != "") {
        vec.push_back({fmt, "", 1});
    }

    // std::cout << m_pattern << std::endl;

    for(auto &v : vec) {
        // std::cout << "(" << std::get<0>(v) << ")--(" << std::get<1>(v) << ")--(" << std::get<2>(v) << ")" << std::endl;

        if(std::get<2>(v) == 0) {
            m_items.push_back(FormatItem::ptr(new StringFormatItem(std::get<0>(v))));
        }
        else {
            auto it = s_format_items.find(std::get<0>(v));
            if(it == s_format_items.end()) {
                // std::cout << "endl" << std::endl;
                m_items.push_back(FormatItem::ptr(new StringFormatItem("<error_format: " + std::get<0>(v) + ">")));
            }
            else {
                // std::cout << std::get<0>(v) << std::endl;
                m_items.push_back(it->second(std::get<1>(v)));
            }
        }
    }

    // std::cout << m_items.size() << std::endl;
}

LoggerManager::LoggerManager() {
    m_root.reset(new Logger);
    m_root->addAppender(LogAppender::ptr(new StdoutLogAppender));
}

Logger::ptr LoggerManager::getLogger(const std::string &name) {
    auto it = m_loggers.find(name);
    if(it != m_loggers.end()) {
        return it->second;
    }
    Logger::ptr logger(new Logger(name));
    logger->m_root = m_root;    // LoggerManager是Logger的友元
    m_loggers[name] = logger;
    return logger;
}

}