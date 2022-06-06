#ifndef __AZURE_ENV_H__
#define __AZURE_ENV_H__

#include <map>
#include <vector>
#include <unistd.h>
#include "singleton.h"
#include "mutex.h"

namespace azure {

class Env {
public:
    typedef RWMutex RWMutexType;

    bool init(int argc, char **argv);

    void add(const std::string &key, const std::string &val);
    bool has(const std::string &key);
    void del(const std::string &key);
    std::string get(const std::string &key, const std::string &default_value="");

    void addHelp(const std::string &key, const std::string &desc);
    void removeHelp(const std::string &key);
    void printHelp();

    const std::string &getExe() const {return m_exe;}
    const std::string &getCwd() const {return m_cwd;}

    bool setEnv(const std::string &key, const std::string & val);
    std::string getEnv(const std::string &key, const std::string &default_value="");

private:
    RWMutexType m_mutex;
    std::map<std::string, std::string> m_args;
    std::vector<std::pair<std::string, std::string>> m_helps;
    std::string m_program;
    /// 程序可执行文件的绝对路径（软链接）
    std::string m_exe;
    /// 进程工作路径
    std::string m_cwd;
};

typedef azure::Singleton<Env> EnvMgr; 

}

#endif