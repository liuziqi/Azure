#include <sys/time.h>
#include <dirent.h>
#include <unistd.h>
#include <string.h>
#include "util.h"
#include "log.h"
#include "fiber.h"

namespace azure {

azure::Logger::ptr g_logger = AZURE_LOG_NAME("system");

pid_t GetThreadId() {
    return syscall(SYS_gettid);
}

// FIXME 获取协程id
uint64_t GetFiberId() {
    return azure::Fiber::GetFiberId();
}

void Backtrace(std::vector<std::string> &bt, int size, int skip) {
    void **array = (void **)malloc(sizeof(void *) * size);
    size_t s = ::backtrace(array, size);

    char **strings = backtrace_symbols(array, s);
    if(strings == NULL) {
        AZURE_LOG_ERROR(g_logger) << "backtrace_symbols error";
        free(strings);
        free(array);
        return;
    }

    for(size_t i = skip; i < s; ++i) {
        bt.push_back(strings[i]);
    }

    free(strings);
    free(array);
}

std::string BacktraceToString(int size, int skip, const std::string &prefix) {
    std::vector<std::string> bt;
    Backtrace(bt, size, skip);
    std::stringstream ss;
    for(size_t i = 0; i < bt.size(); ++i) {
        ss << prefix << bt[i] << std::endl;
    }
    return ss.str();
}

uint64_t GetCurrentMS() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec * 1000ul + tv.tv_usec / 1000;
}

uint64_t GetCurrentUS() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec * 1000 * 1000ul + tv.tv_usec;
}

std::string Time2Str(time_t ts, const std::string &format) {
    struct tm tm;
    localtime_r(&ts, &tm);
    char buf[64];
    strftime(buf, sizeof(buf), format.c_str(), &tm);
    return buf;
}

void FSUtil::ListAllFile(std::vector<std::string> &files, const std::string &path, const std::string subfix) {
    if(access(path.c_str(), 0) != 0) {  // 文件路径不存在
        return;
    }
    DIR *dir = opendir(path.c_str());
    if(dir == nullptr) {
        return;
    }
    struct dirent *dp = nullptr;
    while((dp = readdir(dir)) != nullptr) {
        if(dp->d_type == DT_DIR) {
            if(!strcmp(dp->d_name, ".") || !strcmp(dp->d_name, "..")) {
                continue;
            }
            ListAllFile(files, path + "/" + dp->d_name, subfix);
        }
        else if(dp->d_type == DT_REG) {
            std::string filename(dp->d_name);
            if(subfix.empty()) {
                files.push_back(path + "/" + filename);
            }
            else {
                if(filename.size() < subfix.size()) {
                    continue;
                }
                if(filename.substr(filename.size() - subfix.size()) == subfix) {
                    files.push_back(path + "/" + filename);
                }
            }
        }
    }
    closedir(dir);
}

}