#include "util.h"

namespace azure {

pid_t GetThreadId() {
    return syscall(SYS_gettid);
}

// FIXME 获取协程id
uint32_t GetFiberId() {
    return 0;
}

}