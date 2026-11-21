#include "AsioIOServicePool.h"
#include "Logger.h"

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
    Stop();
    LOGI << "AsioIOServicePool destruct" << std::endl;
}


boost::asio::io_context& AsioIOServicePool::GetIOService(){
    auto& service = _ioServices[_nextIOService++];
    if (_nextIOService == _ioServices.size()) {
        _nextIOService = 0;
    }
    return service;
}

void AsioIOServicePool::Stop(){
    // 幂等：ChatServer / StatusServer 在 main 里已经显式调过一次，
    // 析构函数里那次必须是空操作，否则 join 一个已经 join 过的 thread 会抛 system_error。
    if(_stopped.exchange(true)){
        return;
    }

    //因为仅仅执行work.reset并不能让iocontext从run的状态中退出
    //当iocontext已经绑定了读或写的监听事件后，还需要手动stop该服务。
    for (auto& work_guards : _work_guards) {
        work_guards.reset();
    }

    // 停止所有 io_context，让 run() 返回
    for (auto& ioc : _ioServices) {
        ioc.stop();
    }

    for (auto& t : _threads) {
        if(t.joinable()){
            t.join();
        }
    }
}
