#include "log.h"
#include "config.h"

namespace azure {

const char *LogLevel::ToString(LogLevel::Level level) {
    switch(level) {

#define XX(name) \
    case LogLevel::name: \
        return #name; \
        break;

    XX(DEBUG)
    XX(INFO)
    XX(WARN)
    XX(ERROR)
    XX(FATAL)
#undef XX

    default:
        return "UNKNOWN";
        break;
    }
    return "UNKNOWN";
}

LogLevel::Level LogLevel::FromString(const std::string &str) {
#define XX(level, v) \
    if(str == #v) { \
        return LogLevel::level; \
    }
    XX(DEBUG, debug)
    XX(INFO, info)
    XX(WARN, warn)
    XX(ERROR, error)
    XX(FATAL, fatal)

    XX(DEBUG, DEBUG)
    XX(INFO, INFO)
    XX(WARN, WARN)
    XX(ERROR, ERROR)
    XX(FATAL, FATAL)
    return LogLevel::UNKNOWN;
#undef XX
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

class ThreadNameFormatItem : public LogFormatter::FormatItem {
public:
    ThreadNameFormatItem(const std::string &str="") {}

    void format(std::ostream &os, Logger::ptr logger, LogLevel::Level level, LogEvent::ptr event) override {
        os << event->getThreadName();
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

LogEvent::LogEvent(std::shared_ptr<Logger> logger, LogLevel::Level level, const char *file, int32_t line, uint32_t elapse, 
                    uint32_t threadId, uint32_t fiberId, uint64_t time, const std::string &thread_name)
    : m_logger(logger)
    , m_level(level)
    , m_file(file)
    , m_line(line)
    , m_elapse(elapse)
    , m_threadId(threadId)
    , m_threadName(thread_name)
    , m_fiberId(fiberId)
    , m_time(time) {
}

Logger::Logger(const std::string &name) 
    : m_name(name)
    , m_level(LogLevel::DEBUG) {
    // m_formatter.reset(new LogFormatter("%d %t %F [%p] [%c] %f:%l %m%n"));
    m_formatter.reset(new LogFormatter("%d%T%r%T%t%T%N%T%F%T[%p]%T[%c]%T%f:%l%T%m%n"));
}

void Logger::addAppender(LogAppender::ptr appender) {
    MutexType::Lock lock(m_mutex);
    if(!appender->getFormatter()) {
        // 如果appender没有formatter，就把root的formatter赋给它，但不设置标志位m_hasFormatter
        MutexType::Lock ml(appender->m_mutex);
        appender->m_formatter = m_formatter;
    }
    m_appenders.push_back(appender);
}

void Logger::delAppender(LogAppender::ptr appender) {
    MutexType::Lock lock(m_mutex);
    for(auto it = m_appenders.begin(); it != m_appenders.end(); it++) {
        if(*it == appender) {
            MutexType::Lock ml(appender->m_mutex);
            m_appenders.erase(it);
            break;
        }
    }
}

void Logger::clearAppender() {
    MutexType::Lock lock(m_mutex);
    m_appenders.clear();
}

void Logger::log(LogLevel::Level level, LogEvent::ptr event) {
    if(level >= m_level) {
        auto self = shared_from_this();
        // NOTE 解决了问题：如果logger未定义则用root logger写，否则用定义的logger写，通过判断m_appenders是否为空实现
        // m_appenders不为空，用该logger输出
        MutexType::Lock lock(m_mutex);
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

void Logger::setFormatter(LogFormatter::ptr val) {
    MutexType::Lock lock(m_mutex);
    m_formatter = val;
    for(auto &a : m_appenders) {
        // MutexType::Lock ml(a->m_mutex);  // 死锁
        if(!a->m_hasFormatter) {
            a->setFormatter(val);
        }
    }
}

void Logger::setFormatter(const std::string &val) {
    azure::LogFormatter::ptr new_val(new azure::LogFormatter(val));
    if(new_val->isError()) {
        std::cout << "Logger setFormatter name=" << m_name << " value=" << val << " invalid formatter" << std::endl;
        return;
    }
    setFormatter(new_val);
}

LogFormatter::ptr Logger::getFormatter() {
    MutexType::Lock lock(m_mutex);
    return m_formatter;
}

std::string Logger::toYamlString() {
    MutexType::Lock lock(m_mutex);
    YAML::Node node;
    node["name"] = m_name;
    if(m_level != LogLevel::UNKNOWN) {
        node["level"] = LogLevel::ToString(m_level);
    }
    if(m_formatter) {
        node["formatter"] = m_formatter->getPattern();
    }
    for(auto &i : m_appenders) {
        node["appenders"].push_back(YAML::Load(i->toYamlString()));
    }
    std::stringstream ss;
    ss << node;
    return ss.str();
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

void LogAppender::setFormatter(LogFormatter::ptr val) {
    MutexType::Lock lock(m_mutex);
    m_formatter = val;
    m_hasFormatter = !!m_formatter;
}

LogFormatter::ptr LogAppender::getFormatter() { 
    MutexType::Lock lock(m_mutex);
    return m_formatter;
}

FileLogAppender::FileLogAppender(const std::string &filename) 
    : LogAppender()
    , m_filename(filename) {
    // reopen(); 
}

void FileLogAppender::log(Logger::ptr logger, LogLevel::Level level, LogEvent::ptr event) {
    if(level >= m_level) {
        // 应该互斥访问 m_filestream
        MutexType::Lock lock(m_mutex);
        // TODO 每次写日志都打开一次文件，防止文件被误删，这么写有点蠢
        reopen();
        m_filestream << m_formatter->format(logger, level, event);
    }
}

bool FileLogAppender::reopen() {
    // MutexType::Lock lock(m_mutex);
    if(m_filestream) {
        m_filestream.close();
    }
    m_filestream.open(m_filename, std::ios::out | std::ios::app);
    return !!m_filestream;  // 转换成真正的bool值
}

std::string FileLogAppender::toYamlString() {
    MutexType::Lock lock(m_mutex);
    YAML::Node node;
    node["type"] = "FileLogAppender";
    node["file"] = m_filename;
    if(m_level != LogLevel::UNKNOWN) {
        node["level"] = LogLevel::ToString(m_level);
    }
    if(m_hasFormatter && m_formatter) {
        node["formatter"] = m_formatter->getPattern();
    }
    std::stringstream ss;
    ss << node;
    return ss.str();
}

StdoutLogAppender::StdoutLogAppender()
    : LogAppender() {
}

void StdoutLogAppender::log(Logger::ptr logger, LogLevel::Level level, LogEvent::ptr event) {
    if(level >= m_level) {
        MutexType::Lock lock(m_mutex);
        std::cout << m_formatter->format(logger, level, event);
    }
}

// TODO 优化，这样实现有点傻逼
std::string StdoutLogAppender::toYamlString() {
    MutexType::Lock lock(m_mutex);
    YAML::Node node;
    node["type"] = "StdoutLogAppender";
    if(m_level != LogLevel::UNKNOWN) {
        node["level"] = LogLevel::ToString(m_level);
    }
    if(m_hasFormatter && m_formatter) {
        node["formatter"] = m_formatter->getPattern();
    }
    std::stringstream ss;
    ss << node;
    return ss.str();
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
        XX(N, ThreadNameFormatItem),
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
                        m_error = true;
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
            m_error = true;
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
                m_error = true;
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
    m_loggers[m_root->m_name] = m_root;
    init();
}

Logger::ptr LoggerManager::getLogger(const std::string &name) {
    MutexType::Lock lock(m_mutex);
    auto it = m_loggers.find(name);
    if(it != m_loggers.end()) {
        return it->second;
    }
    Logger::ptr logger(new Logger(name));
    logger->m_root = m_root;    // LoggerManager是Logger的友元
    m_loggers[name] = logger;
    return logger;
}

// Log配置相关的结构体，从配置文件中读取配置选项然后保存到结构体里
struct LogAppenderDefine {
    int type = 0;   // 1 File, 2 Stdout
    LogLevel::Level level = LogLevel::UNKNOWN;
    std::string formatter;
    std::string file;

    bool operator==(const LogAppenderDefine &oth) const {
        return type == oth.type && level == oth.level && formatter == oth.formatter && file == oth.file;
    }
};

// Log配置相关的结构体，从配置文件中读取配置选项然后保存到结构体里
struct LogDefine {
    std::string name;
    LogLevel::Level level;
    std::string formatter;
    std::vector<LogAppenderDefine> appenders;

    bool operator==(const LogDefine &oth) const {
        return name == oth.name && level == oth.level && formatter == oth.formatter && appenders == oth.appenders;
    }

    bool operator<(const LogDefine &oth) const {
        return name < oth.name;
    }
};

// 对LogDefine偏特化，没有按视频里那样对set<LogDefine>偏特化
template<>
class LexicalCast<std::string, LogDefine> {
public:
    LogDefine operator()(const std::string &str) {
        YAML::Node node = YAML::Load(str);
        LogDefine ld;
        ld.name = node["name"].as<std::string>();
        ld.level = LogLevel::FromString(node["level"].IsDefined() ? node["level"].as<std::string>() : "");
        if(node["formatter"].IsDefined()) {
            ld.formatter = node["formatter"].as<std::string>();
        }

        if(node["appenders"].IsDefined()) {
            for(size_t i = 0; i < node["appenders"].size(); ++i) {
                auto a = node["appenders"][i];
                if(!a["type"].IsDefined()) {
                    std::cout << "log config error: appender type is null, " << a << std::endl;
                    continue;
                }
                std::string type = a["type"].as<std::string>();
                LogAppenderDefine lad;
                if(type == "FileLogAppender") {
                    lad.type = 1;
                    if(!a["file"].IsDefined()) {
                        std::cout << "log config error: fileappender file is null, " << a << std::endl;
                        continue;
                    }
                    lad.file = a["file"].as<std::string>();
                    if(a["formatter"].IsDefined()) {
                        lad.formatter = a["formatter"].as<std::string>();
                    }
                }
                else if(type == "StdoutLogAppender") {
                    lad.type = 2;
                }
                else {
                    std::cout << "log config error: appender type is invalid: " << type << std::endl;
                    continue;
                }

                ld.appenders.push_back(lad);
            }
        }
        return ld;
    }
};

template<>
class LexicalCast<LogDefine, std::string> {
public:
    std::string operator()(const LogDefine &ld) {
        YAML::Node node;
        node["name"] = ld.name;
        if(ld.level != LogLevel::UNKNOWN) {
            node["level"] = LogLevel::ToString(ld.level);
        }
        if(!ld.formatter.empty()) {
            node["formatter"] = ld.formatter;
        }
        for(auto &a : ld.appenders) {
            YAML::Node na;
            if(a.type == 1) {
                na["type"] = "FileLogAppender";
                na["file"] = a.file;
            }
            else if(a.type == 2) {
                na["type"] = "StdoutAppender";
            }

            if(a.level != LogLevel::UNKNOWN) {
                na["level"] = LogLevel::ToString(ld.level);
            }

            if(!a.formatter.empty()) {
                na["formatter"] = a.formatter;
            }

            node["appenders"].push_back(na);
        }
        std::stringstream ss;
        ss << node;
        return ss.str();
    }
};

// Log配置保存的地方
azure::ConfigVar<std::set<LogDefine>>::ptr g_log_defines = azure::Config::Lookup("logs", std::set<LogDefine>(), "logs configuration");

// NOTE 静态变量在main函数之前执行，这里的构造函数可以执行一些需要main之前进行的操作
struct LogIniter {
    LogIniter() {
        g_log_defines->addListener([](const std::set<LogDefine> &old_value, const std::set<LogDefine> &new_value) {

            AZURE_LOG_INFO(AZURE_LOG_ROOT()) << "on_logger_conf_changed";

            for(auto &i : new_value) {
                auto it = old_value.find(i);
                azure::Logger::ptr logger;
                if(it == old_value.end()) { // 新的有，旧的没有，新增logger
                    logger = AZURE_LOG_NAME(i.name);    // 新增logger
                }
                else {  // 新的有，旧的也有
                    if(!(i == *it)) {
                        logger = AZURE_LOG_NAME(i.name);    // 两个不等，更新logger
                    }
                }
                // 新增和更新共有的操作
                logger->setLevel(i.level);
                if(!i.formatter.empty()) {
                    logger->setFormatter(i.formatter);
                }
                logger->clearAppender();
                for(auto &a : i.appenders) {
                    azure::LogAppender::ptr ap;
                    if(a.type == 1) {
                        ap.reset(new FileLogAppender(a.file));
                    }
                    else if(a.type == 2) {
                        ap.reset(new StdoutLogAppender);
                    }
                    ap->setLevel(a.level);
                    if(!a.formatter.empty()) {
                        LogFormatter::ptr fmt(new LogFormatter(a.formatter));
                        if(!fmt->isError()) {
                            ap->setFormatter(fmt);
                        }
                        else {
                            std::cout << "logger name=" << i.name 
                                    << ", appender type=" << a.type 
                                    << ", appender formatter=" << a.formatter << " error" << std::endl;
                        }
                    }
                    logger->addAppender(ap);
                }
            }

            for(auto &i : old_value) {
                auto it = new_value.find(i);
                if(it == new_value.end()) { // 旧的有，新的没有，删除logger
                    auto logger = AZURE_LOG_NAME(i.name);
                    logger->setLevel((LogLevel::Level)100);
                    logger->clearAppender();    // 删除了就用root logger输出
                }
            }
        });
    }
};

static LogIniter __log__init;

std::string LoggerManager::toYamlString() {
    MutexType::Lock lock(m_mutex);
    YAML::Node node;
    for(auto &i : m_loggers) {
        node.push_back(YAML::Load(i.second->toYamlString()));
    }
    std::stringstream ss;
    ss << node;
    return ss.str();
}

void LoggerManager::init() {
}

}