#include <time.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include "daemon.h"
#include "log.h"
#include "config.h"

namespace azure {

static azure::Logger::ptr g_logger = AZURE_LOG_NAME("system");

static azure::ConfigVar<uint32_t>::ptr g_daemon_restart_interval = azure::Config::Lookup("daemon.restart_interval", (uint32_t)5, "daemon restart interval");

std::string ProcessInfo::toString() const {
    std::stringstream ss;
    ss << "[ProcessInfo parent_id=" << parent_id
       << " main_id=" << main_id
       << " parent_start_time=" << azure::Time2Str(main_start_time)
       << " restart_count=" << restart_count
       << "]";
    return ss.str();
}

static int real_start(int argc, char **argv, std::function<int (int argc, char **argv)> main_cb) {
    return main_cb(argc, argv);
}

static int real_daemon(int argc, char **argv, std::function<int (int argc, char **argv)> main_cb) {
    daemon(1, 0);   // 不切换工作路径，关闭标准输入输出
    ProcessInfoMgr::GetInstance()->parent_id = getpid();
    ProcessInfoMgr::GetInstance()->parent_start_time = time(0);
    while(true) {
        pid_t pid = fork();
        // 子进程
        if(pid == 0) {  
            ProcessInfoMgr::GetInstance()->main_id = getpid();
            ProcessInfoMgr::GetInstance()->main_start_time = time(0);
            AZURE_LOG_INFO(g_logger) << "process start pid=" << getpid();
            return real_start(argc, argv, main_cb);
        }
        // 出错
        else if(pid < 0) {
            AZURE_LOG_ERROR(g_logger) << "fork fail return=" << pid << " errno=" << errno << " errstr=" << strerror(errno);
            return -1;
        }
        // 父进程
        else {
            int status = 0;
            waitpid(pid, &status, 0);
            if(status) {
                AZURE_LOG_INFO(g_logger) << "child crash pid=" << pid << " status=" << status;
            }
            else {
                AZURE_LOG_INFO(g_logger) << "child finished pid=" << pid;
                break;
            }
            ProcessInfoMgr::GetInstance()->restart_count += 1;
            sleep(g_daemon_restart_interval->getValue());
        }
    }
    return 0;
}

int start_daemon(int argc, char **argv, std::function<int (int argc, char **argv)> main_cb, bool is_daemon) {
    if(!is_daemon) {
        return real_start(argc, argv, main_cb);
    }
    return real_daemon(argc, argv, main_cb);
}

}