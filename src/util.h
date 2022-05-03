#ifndef __AZURE_UTIL_H__
#define __AZURE_UTIL_H__

#include <sys/types.h>
#include <syscall.h>
#include <unistd.h>
#include <cstdint>

namespace azure {

pid_t GetThreadId();
uint32_t GetFiberId();

}

#endif