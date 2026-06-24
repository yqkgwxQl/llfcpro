#include "AsioIOServicePool.h"

AsioIOServicePool::AsioIOServicePool(std::size_t size):
    _ioServices(size),_nextIOService(0){
    _work_guards.reserve(size);
    for(std::size_t i = 0; i < size; i++){
        _work_guards.emplace_back(boost::asio::make_work_guard(_ioServices[i].get_executor()));
    }

    for(std::size_t i = 0; i < _ioServices.size(); i++){
        _threads.emplace_back([this,i](){
            _ioServices[i].run();
        });
    }
}

AsioIOServicePool::~AsioIOServicePool(){
    // Stop();
    std::cout << "AsioIOServicePool destruct" << std::endl;
}


boost::asio::io_context& AsioIOServicePool::GetIOService(){
    auto& service = _ioServices[_nextIOService++];
    if (_nextIOService == _ioServices.size()) {
        _nextIOService = 0;
    }
    return service;
}

void AsioIOServicePool::Stop(){
    //因为仅仅执行work.reset并不能让iocontext从run的状态中退出
    //当iocontext已经绑定了读或写的监听事件后，还需要手动stop该服务。
    for (auto& work_guards : _work_guards) {
        work_guards.reset();
    }

    // ★ 关键修复：停止所有 io_context，让 run() 返回
    for (auto& ioc : _ioServices) {
        ioc.stop();
    }
    
    for (auto& t : _threads) {
        t.join();
    }
}
