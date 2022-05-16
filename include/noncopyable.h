#ifndef __AZURE_NONCOPYABLE_H__
#define __AZURE_NONCOPYABLE_H__

namespace azure {

class Noncpoyable {
public:
    Noncpoyable() = default;
    ~Noncpoyable() = default;
    Noncpoyable(const Noncpoyable&) = delete;
    Noncpoyable& operator=(const Noncpoyable&) = delete;
};

}

#endif