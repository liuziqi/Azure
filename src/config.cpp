#include <sys/types.h>
#include <sys/stat.h>
#include "config.h"
#include "env.h"
#include "log.h"

namespace azure {

static azure::Logger::ptr g_logger = AZURE_LOG_NAME("system");

ConfigVarBase::ptr Config::LookupBase(const std::string name) {
    RWMutexType::ReadLock lock(GetMutex());
    auto it = GetData().find(name);
    return it == GetData().end() ? nullptr : it->second;
}

// A.B = 10, A.C = str
// A:
//      B: 10
//      C: str
// prefix用来表示节点全称，如 A.B, A.C
// node是节点对象，pair类型，fisrt是名称，second是值 
static void ListAllMember(const std::string &prefix, const YAML::Node &node, std::list<std::pair<std::string, const YAML::Node>> &output) {
    // DEBUG yml如果key包含大写会出错
    if(prefix.find_first_not_of("abcdefghijklmnopqrstuvwxyz._0123456789") != std::string::npos) {
        AZURE_LOG_ERROR(AZURE_LOG_ROOT()) << "Config invalid name: " << prefix << " : " << node;
    }
    if(!prefix.empty()) {
        output.push_back(std::make_pair(prefix, node));     // 把节点的全称和节点对象保存下来
    }
    // output.push_back(std::make_pair(prefix, node));
    if(node.IsMap()) {
        for(auto it = node.begin(); it != node.end(); ++it) {
            ListAllMember(prefix.empty() ? it->first.Scalar() : prefix + "." + it->first.Scalar(), it->second, output);
        }
    }
}

void Config::LoadFromYaml(const YAML::Node &root) {
    std::list<std::pair<std::string, const YAML::Node>> all_nodes;
    ListAllMember("", root, all_nodes);

    for(auto &n : all_nodes) {
        std::string key = n.first;
        if(key.empty()) {
            continue;
        }
        std::transform(key.begin(), key.end(), key.begin(), ::tolower);
        // 从配置对象里找该配置项是否存在
        ConfigVarBase::ptr var = LookupBase(key);
        // 如果存在就更新它
        if(var) {
            if(n.second.IsScalar()) {
                var->fromString(n.second.Scalar());
            }
            else {
                std::stringstream ss;
                ss << n.second;
                var->fromString(ss.str());
            }
        }
    }
}

static std::map<std::string, int64_t> s_file2modifytime;
static azure::Mutex s_mutex;

void Config::LoadFromConfDir(const std::string &path) {
    std::string absolute_path = azure::EnvMgr::GetInstance()->getAbsolutePath(path);
    // AZURE_LOG_INFO(g_logger) << "absolute_path=" << absolute_path;
    std::vector<std::string> files;
    FSUtil::ListAllFile(files, absolute_path, ".yml");

    for(auto &f : files) {
        {   
            struct stat st;
            lstat(f.c_str(), &st);
            azure::Mutex::Lock lock(s_mutex);
            if(s_file2modifytime[f] == st.st_mtime) {
                continue;
            }
            s_file2modifytime[f] = st.st_mtime;
        }
        try {
            AZURE_LOG_INFO(g_logger) << "f=" << f;
            YAML::Node root = YAML::LoadFile(f);
            LoadFromYaml(root);
            AZURE_LOG_INFO(g_logger) << "LoadConfFile file=" << f << " ok";
        }
        catch(...) {
            AZURE_LOG_ERROR(g_logger) << "LoadConfFile file=" << f << " failed";
        }
    }
}

void Config::Visit(std::function<void(ConfigVarBase::ptr)> cb) {
    RWMutexType::ReadLock lock(GetMutex());
    ConfigVarMap &data = GetData();
    for(auto it = data.begin(); it != data.end(); ++it) {
        cb(it->second);
    }
}

}