#ifndef __AZURE_SINGLETON_H__
#define __AZURE_SINGLETON_H__

#include <memory>

namespace azure {

template<typename T, typename X=void, int N=0>
class Singleton {
public:
    static T *GetInstance() {
        static T v;
        return &v;
    }
};

template<typename T, typename X=void, int N=0>
class SingletonPtr {
public:
    static std::shared_ptr<T> GetInstance() {
        static std::shared_ptr<T> v(new T);
        return v;
    }
};

} 

#endif