#ifndef __AZURE_MACRO_H__
#define __AZURE_MACRO_H__

#include <string.h>
#include <assert.h>
#include "util.h"

#define AZURE_ASSERT(x) \
    if(!(x)) { \
        AZURE_LOG_ERROR(AZURE_LOG_ROOT()) << "ASSERTION: " #x << "\nbacktrace:\n" << azure::BacktraceToString(100); \
        assert(x); \
    }

#define AZURE_ASSERT2(x, message) \
    if(!(x)) { \
        AZURE_LOG_ERROR(AZURE_LOG_ROOT()) << "ASSERTION: " #x << "\n" << message << "\nbacktrace:\n" << azure::BacktraceToString(100); \
        assert(x); \
    }

#endif