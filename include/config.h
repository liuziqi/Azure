#ifndef __AZURE_CONFIG_H__
#define __AZURE_CONFIG_H__

#include <memory>
#include <sstream>
#include <map>
#include <set>
#include <unordered_map>
#include <unordered_set>
#include <list>
#include <boost/lexical_cast.hpp>   // 实现字符串与目标类型之间的转换
#include <yaml-cpp/yaml.h>
#include <functional>
#include "log.h"
#include "thread.h"

namespace azure {

// 为了实现多态，比如我要在vector里同时保存ConfigVar<int>和ConfigVar<string>的指针
class ConfigVarBase {
public:
    typedef std::shared_ptr<ConfigVarBase> ptr;

    ConfigVarBase(const std::string &name, const std::string &description="")
        : m_name(name)
        , m_description(description) {
        // ::表示后面的对象是全局成员
        std::transform(m_name.begin(), m_name.end(), m_name.begin(), ::tolower);    // key只要小写
    }

    ~ConfigVarBase() {}

    const std::string &getName() const {return m_name;}
    const std::string &getDescription() const {return m_description;}

    virtual std::string toString() = 0;
    virtual bool fromString(const std::string &val) = 0;
    virtual std::string getTypeName() = 0;  // 获取子类模板参数类型名称

protected:
    std::string m_name;
    std::string m_description;
};

// F from_type, T to_type
template<typename F, typename T>
class LexicalCast {
public:
    T operator() (const F &v) {
        return boost::lexical_cast<T>(v);
    }
};

template<typename T>
class LexicalCast<std::string, std::vector<T>> {
public:
    std::vector<T> operator()(const std::string &v) {
        YAML::Node node = YAML::Load(v);
        typename std::vector<T> vec;
        std::stringstream ss;
        for(size_t i = 0; i < node.size(); ++i) {
            ss.str("");
            ss << node[i];
            vec.push_back(LexicalCast<std::string, T>()(ss.str()));
        }
        return vec;
    }
};

template<typename T>
class LexicalCast<std::vector<T>, std::string> {
public:
    std::string operator()(const std::vector<T> &vec) {
        YAML::Node node;
        for(auto &i : vec) {
            node.push_back(YAML::Load(LexicalCast<T, std::string>()(i)));
        }
        std::stringstream ss;
        ss << node;
        return ss.str();
    }
};

template<typename T>
class LexicalCast<std::string, std::list<T>> {
public:
    std::list<T> operator()(const std::string &v) {
        YAML::Node node = YAML::Load(v);
        typename std::list<T> lst;
        std::stringstream ss;
        for(size_t i = 0; i < node.size(); ++i) {
            ss.str("");
            ss << node[i];
            lst.push_back(LexicalCast<std::string, T>()(ss.str()));
        }
        return lst;
    }
};

template<typename T>
class LexicalCast<std::list<T>, std::string> {
public:
    std::string operator()(const std::list<T> &lst) {
        YAML::Node node;
        for(auto &i : lst) {
            node.push_back(YAML::Load(LexicalCast<T, std::string>()(i)));
        }
        std::stringstream ss;
        ss << node;
        return ss.str();
    }
};

template<typename T>
class LexicalCast<std::string, std::set<T>> {
public:
    std::set<T> operator()(const std::string &v) {
        YAML::Node node = YAML::Load(v);
        typename std::set<T> set;
        std::stringstream ss;
        for(size_t i = 0; i < node.size(); ++i) {
            ss.str("");
            ss << node[i];
            set.insert(LexicalCast<std::string, T>()(ss.str()));
        }
        return set;
    }
};

template<typename T>
class LexicalCast<std::set<T>, std::string> {
public:
    std::string operator()(const std::set<T> &set) {
        YAML::Node node;
        for(auto &i : set) {
            node.push_back(YAML::Load(LexicalCast<T, std::string>()(i)));
        }
        std::stringstream ss;
        ss << node;
        return ss.str();
    }
};

template<typename T>
class LexicalCast<std::string, std::unordered_set<T>> {
public:
    std::unordered_set<T> operator()(const std::string &v) {
        YAML::Node node = YAML::Load(v);
        typename std::unordered_set<T> set;
        std::stringstream ss;
        for(size_t i = 0; i < node.size(); ++i) {
            ss.str("");
            ss << node[i];
            set.insert(LexicalCast<std::string, T>()(ss.str()));
        }
        return set;
    }
};

template<typename T>
class LexicalCast<std::unordered_set<T>, std::string> {
public:
    std::string operator()(const std::unordered_set<T> &set) {
        YAML::Node node;
        for(auto &i : set) {
            node.push_back(YAML::Load(LexicalCast<T, std::string>()(i)));
        }
        std::stringstream ss;
        ss << node;
        return ss.str();
    }
};

// map和unordered_map只支持string作为key
template<typename T>
class LexicalCast<std::string, std::map<std::string, T>> {
public:
    std::map<std::string, T> operator()(const std::string &v) {
        YAML::Node node = YAML::Load(v);
        typename std::map<std::string, T> map;
        std::stringstream ss;
        // 这样node也是map
        for(auto it = node.begin(); it != node.end(); ++it) {
            ss.str("");
            // FIXME 只支持简单类型
            ss << it->second;
            map.insert(std::make_pair(it->first.Scalar(), LexicalCast<std::string, T>()(ss.str())));
        }
        return map;
    }
};

template<typename T>
class LexicalCast<std::map<std::string, T>, std::string> {
public:
    std::string operator()(const std::map<std::string, T> &map) {
        YAML::Node node;
        for(auto &i : map) {
            node[i.first] = YAML::Load(LexicalCast<T, std::string>()(i.second));
        }
        std::stringstream ss;
        ss << node;
        return ss.str();
    }
};

template<typename T>
class LexicalCast<std::string, std::unordered_map<std::string, T>> {
public:
    std::unordered_map<std::string, T> operator()(const std::string &v) {
        YAML::Node node = YAML::Load(v);
        typename std::unordered_map<std::string, T> map;
        std::stringstream ss;
        // 这样node也是map
        for(auto it = node.begin(); it != node.end(); ++it) {
            ss.str("");
            // FIXME 目前只支持简单类型
            ss << it->second;
            map.insert(std::make_pair(it->first.Scalar(), LexicalCast<std::string, T>()(ss.str())));
        }
        return map;
    }
};

template<typename T>
class LexicalCast<std::unordered_map<std::string, T>, std::string> {
public:
    std::string operator()(const std::unordered_map<std::string, T> &map) {
        YAML::Node node;
        for(auto &i : map) {
            node[i.first] = YAML::Load(LexicalCast<T, std::string>()(i.second));
        }
        std::stringstream ss;
        ss << node;
        return ss.str();
    }
};

// FromStr: T operator() (const std::string&)
// ToStr: std::string operator() (const T&)
template<typename T, typename FromStr=LexicalCast<std::string, T>, typename ToStr=LexicalCast<T, std::string>>
class ConfigVar : public ConfigVarBase {
public:
    typedef RWMutex RWMutexType;
    typedef std::shared_ptr<ConfigVar> ptr;
    // on_change_cb是返回类型为void，参数类型为两个T的函数类型
    typedef std::function<void (const T &old_value, const T &new_value)> on_change_cb;

    ConfigVar(const std::string &name, const T &default_value, const std::string &description="")
        : ConfigVarBase(name, description)
        , m_val(default_value) {
    }

    std::string toString() override {
        try {
            // return boost::lexical_cast<std::string>(m_val);
            RWMutexType::ReadLock lock(m_mutex);
            return ToStr()(m_val);
        }
        catch(std::exception &e) {
            AZURE_LOG_ERROR(AZURE_LOG_ROOT()) << "ConfigVar::toString exception " << e.what() 
                << " convert: " << typeid(m_val).name() << "to string";
        }
        return "";
    }

    bool fromString(const std::string &val) override {
        try {
            // m_val = boost::lexical_cast<T>(val);
            setValue(FromStr()(val));   // 加过锁了
        }
        catch(std::exception &e) {
            AZURE_LOG_ERROR(AZURE_LOG_ROOT()) << "ConfigVar::toString exception " << e.what() 
                << " convert: string to " << typeid(m_val).name();
        }
        return false;
    }

    const T getValue() {
        RWMutexType::ReadLock lock(m_mutex);
        return m_val;
    }

    void setValue(const T &v) {
        {
            RWMutexType::ReadLock lock(m_mutex);
            if(v == m_val) {
                return;
            }
        }
        // 值发生了变化
        for(auto &i : m_cbs) {
            // 这里就相当于回调执行了若干个函数，每个函数的参数都是这俩
            i.second(m_val, v);
        }

        RWMutexType::WriteLock lock(m_mutex);
        m_val = v;
    }

    std::string getTypeName() override {return typeid(T).name();}

    // 以下函数与配置变更事件相关
    uint64_t addListener(on_change_cb cb) {
        static uint64_t s_fun_id = 0;
        RWMutex::WriteLock lock(m_mutex);    // 写操作
        ++s_fun_id;
        m_cbs[s_fun_id] = cb;
        return s_fun_id;
    }

    void delListener(uint64_t key) {
        RWMutex::WriteLock lock(m_mutex);    // 写操作
        m_cbs.erase(key);
    }

    on_change_cb getListener(uint64_t key) {
        RWMutex::ReadLock lock(m_mutex);    // 读操作
        auto it = m_cbs.find(key);
        return it == m_cbs.end() ? nullptr : it->second;
    }

    void clearListener() {
        RWMutex::WriteLock lock(m_mutex);    // 写操作
        m_cbs.clear();
    }

private:
    RWMutex m_mutex;
    T m_val;

    // NOTE 怎样实现配置变更通知
    // 变更回调函数组，uint64_t key 要求唯一，一般可以用哈希值
    // 用map是因为m_cbs没有判等函数，需要包装一下才能进行删除操作
    std::map<uint64_t, on_change_cb> m_cbs;
};

class Config {
public:
    typedef std::map<std::string, ConfigVarBase::ptr> ConfigVarMap;
    typedef RWMutex RWMutexType;

    template<typename T>
    static typename ConfigVar<T>::ptr Lookup(const std::string &name, const T &default_value, const std::string &description="") {
        RWMutexType::WriteLock lock(GetMutex());
        auto it = GetData().find(name);
        if(it != GetData().end()) {
            auto tmp = std::dynamic_pointer_cast<ConfigVar<T>>(it->second);     // 失败返回nullptr
            if(tmp) {   // key已存在，且值如果能转成与T的指针(说明类型对的上)
                AZURE_LOG_INFO(AZURE_LOG_ROOT()) << "Lookup name=" << name << " exists";
                return tmp;
            }
            else {  // key存在，但值的类型对不上
                AZURE_LOG_ERROR(AZURE_LOG_ROOT()) << "Lookup name=" << name << " exists but type not " << it->second->getTypeName() << ", existing value:" << it->second->toString();
                return nullptr;
            }
        }

        if(name.find_first_not_of("abcdefghijklmnopqrstuvwxyz._0123456789") != std::string::npos) {
            AZURE_LOG_ERROR(AZURE_LOG_ROOT()) << "Lookup name invalid " << name;
            throw std::invalid_argument(name);
        }

        // 这里typename说明ConfigVar<T>::ptr是一个类型而不是变量
        typename ConfigVar<T>::ptr v(new ConfigVar<T>(name, default_value, description));
        GetData()[name] = v;
        return v;
    }

    // 返回派生类指针类型
    template<typename T>
    static typename ConfigVar<T>::ptr Lookup(const std::string &name) {
        RWMutexType::ReadLock lock(GetMutex());
        auto it = GetData().find(name);
        if(it == GetData().end()) {
            return nullptr;
        }
        return std::dynamic_pointer_cast<ConfigVar<T>>(it->second);
    }

    // 返回的是基类指针类型
    static ConfigVarBase::ptr LookupBase(const std::string name);   // 不能返回抽象类对象
    static void LoadFromYaml(const YAML::Node &root);
    static void Visit(std::function<void(ConfigVarBase::ptr)> cb);  // 提供访问m_data的方法

private:
    // NOTE 使用函数的方式获取静态变量，防止其他静态函数使用该静态变量时，因为静态变量的初始化无序性导致访问出错
    static ConfigVarMap &GetData() {
        static ConfigVarMap s_data;
        return s_data;
    }
    // 防止静态变量的初始化无序性导致访问出错
    static RWMutexType &GetMutex() {
        static RWMutexType s_mutex;
        return s_mutex;
    }
};

}

#endif