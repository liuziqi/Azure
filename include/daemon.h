/**
 * @file daemon.h
 * @brief 守护进程
 * @author liuziqi
 * @date 2022-06-02
 * @version 0.1
 * @copyright Copyright (c) 2022
 */
#ifndef __AZURE_DAEMON_H__
#define __AZURE_DAEMON_H__

#include <functional>
#include <unistd.h>
#include "singleton.h"

namespace azure {

struct ProcessInfo {
    /// 父进程id
    pid_t parent_id = 0;
    /// 主进程id
    pid_t main_id = 0;
    /// 父进程启动时间
    uint64_t parent_start_time = 0;
    /// 主进程启动时间
    uint64_t main_start_time = 0;
    /// 主进程重启的次数
    uint32_t restart_count = 0;

    std::string toString() const;
};

typedef azure::Singleton<ProcessInfo> ProcessInfoMgr;

/**
 * @brief 可以选择用守护进程的方式启动程序
 * @param argc 参数个数
 * @param argv 参数值数组
 * @param main_cb 启动函数
 * @param is_daemon 是否以守护进程的方式启动
 * @return int 
 */
int start_daemon(int argc, char **argv, std::function<int (int argc, char **argv)> main_cb, bool is_daemon);

}

#endif