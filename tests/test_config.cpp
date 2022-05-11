#include "config.h"
#include "log.h"
#include <yaml-cpp/yaml.h>

#if 0
azure::ConfigVar<int>::ptr g_int_value_config = azure::Config::Lookup("system.port", (int)8080, "system port");
azure::ConfigVar<float>::ptr g_float_value_config = azure::Config::Lookup("system.port", (float)80.80, "system port");    // 用来测试key相同但类型不同的情况
azure::ConfigVar<std::vector<int>>::ptr g_int_vec_value_config = azure::Config::Lookup("system.int_vec", std::vector<int>{1, 2}, "system int vector");
azure::ConfigVar<std::list<int>>::ptr g_int_list_value_config = azure::Config::Lookup("system.int_list", std::list<int>{1, 2}, "system int list");
azure::ConfigVar<std::set<int>>::ptr g_int_set_value_config = azure::Config::Lookup("system.int_set", std::set<int>{1, 2}, "system int set");
azure::ConfigVar<std::unordered_set<int>>::ptr g_int_unordered_set_value_config = azure::Config::Lookup("system.int_unordered_set", std::unordered_set<int>{1, 2}, "system int unordered set");
azure::ConfigVar<std::map<std::string, int>>::ptr g_str_int_map_value_config = azure::Config::Lookup("system.str_int_map", std::map<std::string, int>{{"k", 2}}, "system string int map");
azure::ConfigVar<std::unordered_map<std::string, int>>::ptr g_str_int_unordered_map_value_config = azure::Config::Lookup("system.str_int_unordered_map", std::unordered_map<std::string, int>{{"k", 2}}, "system string int unordered map");

void print_yaml(const YAML::Node &node, int level) {
    if(node.IsScalar()) {
        AZURE_LOG_INFO(AZURE_LOG_ROOT()) << std::string(level * 4, ' ') << node.Scalar() << " - " << node.Type() << " - " << level;
    } 
    else if(node.IsNull()) {
        AZURE_LOG_INFO(AZURE_LOG_ROOT()) << std::string(level * 4, ' ') << "NULL - " << node.Type() << " - " << level;
    }
    else if(node.IsMap()) {
        for(auto it = node.begin(); it != node.end(); ++it) {
            AZURE_LOG_INFO(AZURE_LOG_ROOT()) << std::string(level * 4, ' ') << it->first << " - " << it->second.Type() << " - " << level;
            print_yaml(it->second, level + 1);
        }
    }
    else if(node.IsSequence()) {
        for(size_t i = 0; i < node.size(); ++i) {
            AZURE_LOG_INFO(AZURE_LOG_ROOT()) << std::string(level * 4, ' ') << i << " - " << node[i].Type() << " - " << level;
            print_yaml(node[i], level + 1);
        }
    }
}

void test_yaml() {
    YAML::Node root = YAML::LoadFile("/home/lzq/Azure/cfg/test_cfg.yml");
    print_yaml(root, 0);
    // AZURE_LOG_INFO(AZURE_LOG_ROOT()) << root;
}

void test_config() {
    AZURE_LOG_INFO(AZURE_LOG_ROOT()) << "before: " << g_int_value_config->getValue();
    AZURE_LOG_INFO(AZURE_LOG_ROOT()) << "before: " << g_int_value_config->toString();

// {}防止v出现重定义
#define XX(g_var, name, prefix) \
    { \
        auto &v = g_var->getValue(); \
        for(auto &i : v) { \
            AZURE_LOG_INFO(AZURE_LOG_ROOT()) << #prefix " " #name ": " << i; \
        } \
        AZURE_LOG_INFO(AZURE_LOG_ROOT()) << #prefix " " #name " yaml: \n" << g_var->toString(); \
    }

#define XX_M(g_var, name, prefix) \
    { \
        auto &v = g_var->getValue(); \
        for(auto &i : v) { \
            AZURE_LOG_INFO(AZURE_LOG_ROOT()) << #prefix " " #name ": {" << i.first << ", " << i.second << "}"; \
        } \
        AZURE_LOG_INFO(AZURE_LOG_ROOT()) << #prefix " " #name " yaml: \n" << g_var->toString(); \
    }

    XX(g_int_vec_value_config, int_vec, before);
    XX(g_int_list_value_config, int_list, before);
    XX(g_int_set_value_config, int_set, before);
    XX(g_int_unordered_set_value_config, int_unordered_set, before);
    XX_M(g_str_int_map_value_config, str_int_map, before);
    XX_M(g_str_int_unordered_map_value_config, str_int_unordered_map, before);

    YAML::Node root = YAML::LoadFile("/home/lzq/Azure/cfg/test_cfg.yml");
    azure::Config::LoadFromYaml(root);

    AZURE_LOG_INFO(AZURE_LOG_ROOT()) << "after: " << g_int_value_config->getValue();
    AZURE_LOG_INFO(AZURE_LOG_ROOT()) << "after: " << g_int_value_config->toString();

    XX(g_int_vec_value_config, int_vec, after);
    XX(g_int_list_value_config, int_list, after);
    XX(g_int_set_value_config, int_set, after);
    XX(g_int_unordered_set_value_config, int_unordered_set, after);
    XX_M(g_str_int_map_value_config, str_int_map, after);
    XX_M(g_str_int_unordered_map_value_config, str_int_unordered_map, after);
#undef XX
}

class Person {
public:
    std::string m_name = "";
    int m_age = 0;
    bool m_sex = true;

    std::string toString() const {
        std::stringstream ss;
        ss << "[Person name=" << m_name << " age=" << m_age << " sex=" << m_sex << "]";
        return ss.str();
    }
    
    bool operator==(const Person &p) const {    // 这里不加const会出错
        return m_name == p.m_name && m_age == p.m_age && m_sex == p.m_sex;
    }
};

namespace azure {

// LEARN 了解下模板偏特化相关的知识
// 模板偏特化，不需要模板参数了
template<>
class LexicalCast<std::string, Person> {
public:
    Person operator()(const std::string &v) {
        YAML::Node node = YAML::Load(v);
        Person p;
        p.m_name = node["name"].as<std::string>();
        p.m_age = node["age"].as<int>();
        p.m_sex = node["sex"].as<bool>();
        return p;
    }
};

template<>
class LexicalCast<Person, std::string> {
public:
    std::string operator()(const Person &p) {
        YAML::Node node;
        node["name"] = p.m_name;
        node["age"] = p.m_age;
        node["sex"] = p.m_sex;
        std::stringstream ss;
        ss << node;
        return ss.str();
    }
};

}

azure::ConfigVar<Person>::ptr g_person = azure::Config::Lookup("class.person", Person(), "system person");
azure::ConfigVar<std::map<std::string, Person>>::ptr g_person_map = azure::Config::Lookup("class.person_map", 
                                                                        std::map<std::string, Person>(), "system person map");
azure::ConfigVar<std::map<std::string, std::vector<Person>>>::ptr g_person_vec_map = azure::Config::Lookup("class.person_vec_map", 
                                                                        std::map<std::string, std::vector<Person>>(), "system person vector map");                                                                      

void test_class() {
    AZURE_LOG_INFO(AZURE_LOG_ROOT()) << "before: " << g_person->getValue().toString() << "\n" << g_person->toString();

#define XX_PM(g_var, prefix) \
    { \
        auto m = g_var->getValue(); \
        for(auto &i : m) { \
            AZURE_LOG_INFO(AZURE_LOG_ROOT()) << prefix << ": " << i.first << " - " << i.second.toString(); \
        } \
        AZURE_LOG_INFO(AZURE_LOG_ROOT()) << prefix << ": size=" << m.size(); \
    }

    // 触发配置变更事件
    g_person->addListener([](const Person &old_value, const Person &new_value) {
        AZURE_LOG_INFO(AZURE_LOG_ROOT()) <<"old_value=" << old_value.toString() << " new_value=" << new_value.toString();
    });

    XX_PM(g_person_map, "class.person_map before")

    AZURE_LOG_INFO(AZURE_LOG_ROOT()) << "before: g_person_vec_map size=" << g_person_vec_map->getValue().size();
    AZURE_LOG_INFO(AZURE_LOG_ROOT()) << "before: g_person_vec_map:\n" << g_person_vec_map->toString();

    YAML::Node root = YAML::LoadFile("/home/lzq/Azure/cfg/test_cfg.yml");
    azure::Config::LoadFromYaml(root);

    AZURE_LOG_INFO(AZURE_LOG_ROOT()) << "after: " << g_person->getValue().toString() << "\n" << g_person->toString();

    XX_PM(g_person_map, "class.person_map after")

    AZURE_LOG_INFO(AZURE_LOG_ROOT()) << "after: g_person_vec_map size=" << g_person_vec_map->getValue().size();
    AZURE_LOG_INFO(AZURE_LOG_ROOT()) << "after: g_person_vec_map:\n" << g_person_vec_map->toString();
#undef XX_PM
}
#endif

// 加载文件后会触发配置变化 --> 触发事件 --> 变更配置
void test_log() {
    static azure::Logger::ptr system_log = AZURE_LOG_NAME("system");    // 刚开始不存在
    AZURE_LOG_INFO(system_log) << "hello system";

    std::cout << "before:\n" << azure::LoggerMgr::GetInstance()->toYamlString() << std::endl;
    YAML::Node root = YAML::LoadFile("/home/lzq/Azure/cfg/log_cfg.yml");
    azure::Config::LoadFromYaml(root);
    std::cout << "===================================" << std::endl;
    std::cout << "after:\n" << azure::LoggerMgr::GetInstance()->toYamlString() << std::endl;

    AZURE_LOG_INFO(system_log) << "hello system";   // system_log这时加载进来了

    // 修改logger的formatter时，如果appender没有自己的formatter，也要随之改变
    system_log->setFormatter("%d - %m%n");
    AZURE_LOG_INFO(system_log) << "hello system";
}

int main(int argc, char **argv) {
    // test_yaml();
    // test_config();
    // test_class();
    // test_log();

    // 测试 azure::Config::Visit 方法
    // azure::Config::Visit([](azure::ConfigVarBase::ptr var) {
    //     AZURE_LOG_INFO(AZURE_LOG_ROOT()) << "name=" << var->getName() 
    //                             << " description=" << var->getDescription()
    //                             << " typename=" << var->getTypeName()
    //                             << " value=" << var->toString();
    // });

    YAML::Node root = YAML::LoadFile("/home/lzq/Azure/cfg/log_cfg.yml");
    azure::Config::LoadFromYaml(root);

    return 0;
}