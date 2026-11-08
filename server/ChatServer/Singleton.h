#pragma once
#include <memory>
#include <iostream>
#include <mutex>

template<class T>
class Singleton{

protected:
    Singleton() = default;
    Singleton(const Singleton<T>& ) = delete;
    Singleton<T>& operator=(const Singleton<T>& ) = delete;
    static std::shared_ptr<T> _instance;
    ~Singleton() {
        std::cout << "this is singleton destruct" << std::endl;
    }
public:
    static std::shared_ptr<T> GetInstance(){
        static std::once_flag s_flag;
        std::call_once(s_flag,[&](){
            /* 问题原因：
            * Singleton::GetInstance() 内部调用 std::make_shared<T>() 时，标准库内部的 _Construct
            * 函数尝试调用 LogicSystem 的默认构造函数，但该构造函数是 private 的。
            *
            * 虽然 Singleton<LogicSystem> 是 LogicSystem 的友元类，但 std::make_shared 是一个
            * 外部函数模板，其内部实现函数（如 _Construct、_Sp_counted_ptr_inplace）并不是
            * LogicSystem 的友元，因此无法访问其私有构造函数。
            *
            * 解决方案：
            * 将 std::make_shared<T>() 替换为 std::shared_ptr<T>(new T())。
            *
            * 原因：new T() 直接在 Singleton::GetInstance() 内部执行，而 Singleton<T> 是
            * LogicSystem 的友元，因此可以访问其私有构造函数。*/
            _instance = std::shared_ptr<T>(new T());
        });
        return _instance;
    }
    void PrintAddress(){
        std::cout << _instance.get() << std::endl;
    }
};

template<class T>
std::shared_ptr<T> Singleton<T>::_instance = nullptr;