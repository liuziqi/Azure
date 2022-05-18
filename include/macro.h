#ifndef __AZURE_MACRO_H__
#define __AZURE_MACRO_H__

#include <string.h>
#include <assert.h>
#include "util.h"

#if defined __GNUC__ || defined __llvm__
#define AZURE_LIKELY(x)    __builtin_expect(!!(x), 1)   // 告诉编译器发生的概率大，应该和分支预测相关
#define AZURE_UNLIKELY(x)  __builtin_expect(!!(x), 0)
#else
#define AZURE_LIKELY(x)    (x)
#define AZURE_UNLIKELY(x)  (x)
#endif

#define AZURE_ASSERT(x) \
    if(AZURE_UNLIKELY(!(x))) { \
        AZURE_LOG_ERROR(AZURE_LOG_ROOT()) << "ASSERTION: " #x << "\nbacktrace:\n" << azure::BacktraceToString(100); \
        assert(x); \
    }

#define AZURE_ASSERT2(x, message) \
    if(AZURE_UNLIKELY(!(x))) { \
        AZURE_LOG_ERROR(AZURE_LOG_ROOT()) << "ASSERTION: " #x << "\n" << message << "\nbacktrace:\n" << azure::BacktraceToString(100); \
        assert(x); \
    }

#endif