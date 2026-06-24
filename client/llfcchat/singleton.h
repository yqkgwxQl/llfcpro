#ifndef SINGLETON_H
#define SINGLETON_H
#include "global.h"
#include <iostream>
#include <memory>
#include <mutex>

template<class T>
class Singleton{
    //protected： 允许子类能够使用父类的构造函数
protected:
    Singleton() = default;
    Singleton(const Singleton<T>&) = delete;
    Singleton<T>& operator = (const Singleton<T>&) = delete;
    static std::shared_ptr<T> _instance;
public:
    static std::shared_ptr<T> GetInstance(){
        static std::once_flag s_flag;
        std::call_once(s_flag,[&](){
            _instance = std::shared_ptr<T>(new T);
        });
        return _instance;
    }
    void PrintAdress(){
        //它用于获取 std::shared_ptr 智能指针内部所管理的原始指针（裸指针）的地址。
        std::cout << _instance.get();
    }
    ~Singleton(){
        std::cout << "this is singleton destruct" << std::endl;
    }
};

template<class T>
std::shared_ptr<T> Singleton<T>::_instance = nullptr;
#endif // SINGLETON_H
