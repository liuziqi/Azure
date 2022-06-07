/**
 * @file config.h
 * @brief 配置模块
 * @author liuziqi
 * @date 2022-05-31
 * @version 0.1
 * @copyright Copyright (c) 2022
 */
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
#include "mutex.h"

namespace azure {

/**
 * @brief 配置变量的基类
 */
class ConfigVarBase {
public:
    typedef std::shared_ptr<ConfigVarBase> ptr;

    /**
     * @brief 构造函数
     * @param name 配置参数名称[0-9a-z_.]
     * @param description 配置参数描述
     */
    ConfigVarBase(const std::string &name, const std::string &description="")
        : m_name(name)
        , m_description(description) {
        // ::表示后面的对象是全局成员
        std::transform(m_name.begin(), m_name.end(), m_name.begin(), ::tolower);
    }

    /**
     * @brief 析构函数
     */
    virtual ~ConfigVarBase() {}

    /**
     * @brief 获取配置参数名称
     */
    const std::string &getName() const {return m_name;}

    /**
     * @brief 获取配置参数的描述
     */
    const std::string &getDescription() const {return m_description;}

    /**
     * @brief 将配置转换成字符串
     */
    virtual std::string toString() = 0;

    /**
     * @brief 从字符串初始化值
     */
    virtual bool fromString(const std::string &val) = 0;

    /**
     * @brief 返回配置参数值的类型名称
     */
    virtual std::string getTypeName() = 0;

protected:
    /// 配置参数的名称
    std::string m_name;
    /// 配置参数的描述
    std::string m_description;
};

/**
 * @brief 类型转换模板类 (F源类型, T目标类型)
 */
template<typename F, typename T>
class LexicalCast {
public:
    /**
     * @brief 类型转换
     * @param v 源类型值
     * @return T v对应的目标类型值
     * @exception 当类型不可转换时抛出异常
     */
    T operator() (const F &v) {
        return boost::lexical_cast<T>(v);
    }
};

/**
 * @brief 类型转换模板类偏特化(YAML String 转换成 std::vector<T>)
 */
template<typename T>
class LexicalCast<std::string, std::vector<T>> {
public:
    /**
     * @brief 类型转换
     * @param v YAML String类型
     * @return std::vector<T> 目标类型的值
     * @exception 当类型不可转换时抛出异常
     */
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

/**
 * @brief 类型转换模板类偏特化(std::vector<T> 转换成 YAML String)
 */
template<typename T>
class LexicalCast<std::vector<T>, std::string> {
public:
    /**
     * @brief 类型转换
     * @param vec 源类型值
     * @return std::string YAML String类型
     */
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

/**
 * @brief 类型转换模板类偏特化(YAML String 转换成 std::list<T>)
 */
template<typename T>
class LexicalCast<std::string, std::list<T>> {
public:
    /**
     * @brief 类型转换
     * @param v YAML String类型
     * @return std::list<T> 目标类型的值
     */
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

/**
 * @brief 类型转换模板类偏特化(std::list<T> 转换成 YAML String)
 */
template<typename T>
class LexicalCast<std::list<T>, std::string> {
public:
    /**
     * @brief 类型转换
     * @param lst 源类型值
     * @return std::string YAML String类型
     */
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

/**
 * @brief 类型转换模板类偏特化(YAML String 转换成 std::set<T>)
 */
template<typename T>
class LexicalCast<std::string, std::set<T>> {
public:
    /**
     * @brief 类型转换
     * @param v YAML String类型
     * @return std::set<T> 目标类型的值
     */
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

/**
 * @brief 类型转换模板类偏特化(std::set<T> 转换成 YAML String)
 */
template<typename T>
class LexicalCast<std::set<T>, std::string> {
public:
    /**
     * @brief 类型转换
     * @param set 源类型值
     * @return std::string YAML String类型
     */
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

/**
 * @brief 类型转换模板类偏特化(YAML String 转换成 std::unordered_set<T>)
 */
template<typename T>
class LexicalCast<std::string, std::unordered_set<T>> {
public:
    /**
     * @brief 类型转换
     * @param v YAML String类型
     * @return std::unordered_set<T> 目标类型的值
     */
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

/**
 * @brief 类型转换模板类偏特化(std::unordered_set<T> 转换成 YAML String)
 */
template<typename T>
class LexicalCast<std::unordered_set<T>, std::string> {
public:
    /**
     * @brief 类型转换
     * @param set 源类型值
     * @return std::string YAML String类型
     */
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

/**
 * @brief 类型转换模板类片特化(YAML String 转换成 std::map<std::string, T>)
 */
template<typename T>
class LexicalCast<std::string, std::map<std::string, T>> {
public:
    /**
     * @brief 类型转换
     * @param v YAML String类型
     * @return std::map<std::string, T> 目标类型的值
     */
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


/**
 * @brief 类型转换模板类片特化(std::map<std::string, T> 转换成 YAML String)
 */
template<typename T>
class LexicalCast<std::map<std::string, T>, std::string> {
public:
    /**
     * @brief 类型转换
     * @param map 源类型的值
     * @return std::string YAML String类型
     */
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

/**
 * @brief 类型转换模板类片特化(YAML String 转换成 std::unordered_map<std::string, T>)
 */
template<typename T>
class LexicalCast<std::string, std::unordered_map<std::string, T>> {
public:
    /**
     * @brief 类型转换
     * @param v YAML String类型
     * @return std::unordered_map<std::string, T> 目标类型的值 
     */
    std::unordered_map<std::string, T> operator()(const std::string &v) {
        YAML::Node node = YAML::Load(v);
        typename std::unordered_map<std::string, T> map;
        std::stringstream ss;
        for(auto it = node.begin(); it != node.end(); ++it) {
            ss.str("");
            // FIXME 目前只支持简单类型
            ss << it->second;
            map.insert(std::make_pair(it->first.Scalar(), LexicalCast<std::string, T>()(ss.str())));
        }
        return map;
    }
};

/**
 * @brief 类型转换模板类片特化(std::unordered_map<std::string, T> 转换成 YAML String)
 */
template<typename T>
class LexicalCast<std::unordered_map<std::string, T>, std::string> {
public:
    /**
     * @brief 类型转换
     * @param map 源类型的值
     * @return std::string YAML String类型
     */
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

/**
 * @brief 配置参数模板子类，保存对应类型的参数值
 * @tparam T 参数的具体类型
 * @tparam FromStr 从std::string转换成T类型的仿函数
 * @tparam ToStr 从T转换成std::string的仿函数
 * @tparam std::string YAML格式的字符串
 */
template<typename T, typename FromStr=LexicalCast<std::string, T>, typename ToStr=LexicalCast<T, std::string>>
class ConfigVar : public ConfigVarBase {
public:
    typedef RWMutex RWMutexType;
    typedef std::shared_ptr<ConfigVar> ptr;
    typedef std::function<void (const T &old_value, const T &new_value)> on_change_cb;

    /**
     * @brief 构造函数
     * @param name 参数名称有效字符为[0-9a-z_.]
     * @param default_value 参数的默认值
     * @param description 参数的描述
     */
    ConfigVar(const std::string &name, const T &default_value, const std::string &description="")
        : ConfigVarBase(name, description)
        , m_val(default_value) {
    }

    /**
     * @brief 将参数值转换成YAML String
     * @exception 当转换失败抛出异常
     */
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

    /**
     * @brief 从YAML String转成参数的值
     * @exception 当转换失败抛出异常
     */
    bool fromString(const std::string &val) override {
        try {
            // m_val = boost::lexical_cast<T>(val);
            setValue(FromStr()(val));   // 加过锁了
        }
        catch(std::exception &e) {
            AZURE_LOG_ERROR(AZURE_LOG_ROOT()) << "ConfigVar::fromString exception " << e.what() 
                << " convert: string to " << typeid(m_val).name();
        }
        return false;
    }

    /**
     * @brief 获取当前参数的值
     */
    const T getValue() {
        RWMutexType::ReadLock lock(m_mutex);
        return m_val;
    }

    /**
     * @brief 设置当前参数的值
     * @details 如果参数的值有发生变化,则通知对应的注册回调函数
     */
    void setValue(const T &v) {
        {
            RWMutexType::ReadLock lock(m_mutex);
            if(v == m_val) {
                return;
            }
        }
        for(auto &i : m_cbs) {
            // 这里就相当于回调执行了若干个函数，每个函数的参数都是这俩
            i.second(m_val, v);
        }

        RWMutexType::WriteLock lock(m_mutex);
        m_val = v;
    }

    /**
     * @brief 返回参数值的类型名称(typeinfo)
     */
    std::string getTypeName() override {return typeid(T).name();}

    /**
     * @brief 添加变化回调函数
     * @return 返回该回调函数对应的唯一id,用于删除回调
     */
    uint64_t addListener(on_change_cb cb) {
        static uint64_t s_fun_id = 0;
        RWMutex::WriteLock lock(m_mutex);       // 写操作
        ++s_fun_id;
        m_cbs[s_fun_id] = cb;
        return s_fun_id;
    }

    /**
     * @brief 删除回调函数
     * @param key 回调函数的唯一id
     */
    void delListener(uint64_t key) {
        RWMutex::WriteLock lock(m_mutex);       // 写操作
        m_cbs.erase(key);
    }

    /**
     * @brief 获取回调函数
     * @param key 回调函数的唯一id
     * @return 如果存在返回对应的回调函数,否则返回nullptr
     */
    on_change_cb getListener(uint64_t key) {
        RWMutex::ReadLock lock(m_mutex);        // 读操作
        auto it = m_cbs.find(key);
        return it == m_cbs.end() ? nullptr : it->second;
    }

    /**
     * @brief 清除所有的回调函数
     */
    void clearListener() {
        RWMutex::WriteLock lock(m_mutex);    // 写操作
        m_cbs.clear();
    }

private:
    RWMutex m_mutex;
    T m_val;
    //用map是因为m_cbs没有判等函数，需要包装一下才能进行删除操作
    std::map<uint64_t, on_change_cb> m_cbs;
};


/**
 * @brief ConfigVar的管理类
 * @details 提供便捷的方法创建/访问ConfigVar
 */
class Config {
public:
    typedef std::map<std::string, ConfigVarBase::ptr> ConfigVarMap;
    typedef RWMutex RWMutexType;

    /**
     * @brief 获取/创建对应参数名的配置参数
     * @param name 配置参数名称
     * @param default_value 参数默认值
     * @param description 参数描述
     * @return ConfigVar<T>::ptr 指向配置项的shared_ptr，如果参数名存在但是类型不匹配则返回nullptr
     * @details 获取参数名为name的配置参数,如果存在直接返回，如果不存在,创建参数配置并用default_value赋值
     * @exception 如果参数名包含非法字符[^0-9a-z_.] 抛出异常 std::invalid_argument
     */
    template<typename T>
    static typename ConfigVar<T>::ptr Lookup(const std::string &name, const T &default_value, const std::string &description="") {
        RWMutexType::WriteLock lock(GetMutex());
        auto it = GetData().find(name);
        if(it != GetData().end()) {
            auto tmp = std::dynamic_pointer_cast<ConfigVar<T>>(it->second);     // dynamic_pointer_cast失败返回nullptr
            if(tmp) {   // key已存在，且值如果能转成与T的指针(说明类型对的上)
                AZURE_LOG_INFO(AZURE_LOG_ROOT()) << "Lookup name=" << name << " exists";
                return tmp;
            }
            else {      // key存在，但值的类型对不上
                AZURE_LOG_ERROR(AZURE_LOG_ROOT()) << "Lookup name=" << name << " exists but type not " << it->second->getTypeName() << ", existing value:" << it->second->toString();
                return nullptr;
            }
        }

        if(name.find_first_not_of("abcdefghijklmnopqrstuvwxyz._0123456789") != std::string::npos) {
            AZURE_LOG_ERROR(AZURE_LOG_ROOT()) << "Lookup name invalid " << name;
            throw std::invalid_argument(name);
        }

        // NOTE 这里typename说明ConfigVar<T>::ptr是一个类型而不是变量
        typename ConfigVar<T>::ptr v(new ConfigVar<T>(name, default_value, description));
        GetData()[name] = v;
        return v;
    }

    /**
     * @brief 查找配置参数
     * @param name 配置参数名称
     * @return ConfigVar<T>::ptr 返回配置参数名为name的配置参数
     */
    template<typename T>
    static typename ConfigVar<T>::ptr Lookup(const std::string &name) {
        RWMutexType::ReadLock lock(GetMutex());
        auto it = GetData().find(name);
        if(it == GetData().end()) {
            return nullptr;
        }
        return std::dynamic_pointer_cast<ConfigVar<T>>(it->second);
    }

    /**
     * @brief 查找配置参数,返回配置参数的基类
     * @param name 配置参数名称
     */
    static ConfigVarBase::ptr LookupBase(const std::string name);

    /**
     * @brief 使用YAML::Node初始化配置模块
     */
    static void LoadFromYaml(const YAML::Node &root);

    /**
     * @brief 加载文件夹中的配置项
     */
    static void LoadFromConfDir(const std::string &path);

    /**
     * @brief 遍历配置模块里面所有配置项
     * @param cb 配置项回调函数
     */
    static void Visit(std::function<void(ConfigVarBase::ptr)> cb);

private:
    // NOTE 使用函数的方式获取静态变量，防止其他静态函数使用该静态变量时，因为静态变量的初始化无序性导致访问出错

    /**
     * @brief 返回所有的配置项
     */
    static ConfigVarMap &GetData() {
        static ConfigVarMap s_data;
        return s_data;
    }
    
    /**
     * @brief 配置项的RWMutex
     */
    static RWMutexType &GetMutex() {
        static RWMutexType s_mutex;
        return s_mutex;
    }
};

}

#endif