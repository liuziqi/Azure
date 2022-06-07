#ifndef __AZURE_UTIL_H__
#define __AZURE_UTIL_H__

#include <sys/types.h>
#include <syscall.h>
#include <unistd.h>
#include <cstdint>
#include <vector>
#include <execinfo.h>
#include <string>

namespace azure {

pid_t GetThreadId();
uint64_t GetFiberId();

void Backtrace(std::vector<std::string> &bt, int size=64, int skip=1);
std::string BacktraceToString(int size=64, int skip=2, const std::string &prefix="\t");

// 事件ms
uint64_t GetCurrentMS();
uint64_t GetCurrentUS();

std::string Time2Str(time_t ts=time(0), const std::string &format="%Y-%m-%d %H:%M:%S");

class FSUtil {
public:
    static void ListAllFile(std::vector<std::string> &files, const std::string &path, const std::string subfix);
};

}

#endif