#include "RedisConPool.h"
#include "Logger.h"
#include <iostream>

RedisConPool::RedisConPool(const char* host,const int port,const std::size_t poolSize,const char* pwd)
    :host_(host),port_(port),poolSize_(poolSize),b_stop_(false){
        for(std::size_t i = 0; i < poolSize_; i++){
            auto context = redisConnect(host,port);
            if(context == nullptr || context->err != 0){
                if (context != nullptr) {
                    redisFree(context);
                }
                continue;
        }

        auto reply = (redisReply*)redisCommand(context,"AUTH %s",pwd);
        if(reply->type == REDIS_REPLY_ERROR){
            LOGI<<"认证失败"<< std::endl;
            //执行成功释放redisCommand执行后返回的redisReply所占用的内存
            freeReplyObject(reply);
            continue;
        }

        //执行成功释放redisCommand执行后返回的redisReply所占用的内存
        freeReplyObject(reply);
        LOGI <<"认证成功"<< std::endl;
        RedisContextPtr ptr(context);
        connections_.push(std::move(ptr));
    }
}   
RedisConPool::~RedisConPool(){
    std::lock_guard<std::mutex> guard_lock(mutex_);
    Close();
    while(!connections_.empty()){
        connections_.pop();
    }
}
/* 这样写会造成死锁
RedisConPool::~RedisConPool(){
    std::lock_guard<std::mutex> guard_lock(mutex_);
    Close();
    while(!connections_.empty()){
        connections_.pop();
    }
}*/
//原子的，不需要锁
void RedisConPool::Close(){
    b_stop_ = true;
    cond_.notify_all();
}

RedisContextPtr RedisConPool::GetRedisConnection(){
    std::unique_lock<std::mutex> unique_lock(mutex_);
    cond_.wait(unique_lock,[this](){
        if(b_stop_)
            return true;
        return !connections_.empty();
    });
    if(b_stop_)
        return nullptr;
    auto context = std::move(connections_.front());
    connections_.pop();
    return context;
}

void RedisConPool::ReturnRedisConnection(RedisContextPtr ptr){
    {
        std::lock_guard<std::mutex> guard_lock(mutex_);
        if(b_stop_)
            return;
        connections_.push(std::move(ptr));
    }
    cond_.notify_one();
}
/*请问条件变量去唤醒不需要锁，条件变量的等待需要锁，对吗？为什么？
    1.为什么 wait 需要锁？ ————>  wait 做的事情本质上是三步原子操作：
    ① 释放锁             ← 让其他线程能进临界区
    ② 把自己挂起（休眠）  ← 等待被唤醒
    ③ 被唤醒后重新获取锁 ← 继续执行
    锁的存在保证了"检查条件"和"开始等待"之间不会被别的线程插队。wait 内部是先释放锁再休眠，所以别的线程能进去放数据并 notify，
    等它被唤醒后又会重新拿到锁再看条件。
    2.为什么 notify 不需要锁？
    notify_one / notify_all 只是发一个信号给等待的线程："可以醒醒了"。它不读写任何共享数据，所以不需要锁
*/


/*"万一这时候还有redis的服务被使用呢？"
    答：不会误杀。因为：
        借出流程：队列 → pop 移除 → 交给线程        ← 已经在用的不在队列里
        清空流程：                     队列里的 pop → 只清空闲的
        归还流程：线程用完 → 如果已关闭 → 归还时自动 redisFree（线程已用完，安全）*/
void RedisConPool::ClearConnections(){
    std::lock_guard<std::mutex> lock(mutex_);
    while (!connections_.empty()) {
        auto& context = connections_.front();
        context.reset();
        connections_.pop();
    } 
}
//这个函数和上个函数没有任何区别
// void RedisConPool::ClearConnections(){
//     std::lock_guard<std::mutex> lock(mutex_);
//     while (!connections_.empty()) {
//         connections_.pop();
//     } 
// }
