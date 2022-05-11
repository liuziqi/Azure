#include "config.h"

namespace azure {

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

void Config::Visit(std::function<void(ConfigVarBase::ptr)> cb) {
    RWMutexType::ReadLock lock(GetMutex());
    ConfigVarMap &data = GetData();
    for(auto it = data.begin(); it != data.end(); ++it) {
        cb(it->second);
    }
}

}