#include "RedisConPool.h"
#include "Logger.h"
#include <iostream>
#include <chrono>

RedisConPool::RedisConPool(const char* host,const int port,const std::size_t poolSize,const char* pwd)
    :host_(host),port_(port),poolSize_(poolSize),b_stop_(false),counter_(0),pwd_(pwd){
        for(std::size_t i = 0; i < poolSize_; i++){
            auto context = redisConnect(host,port);
            if(context == nullptr || context->err != 0){
                if (context != nullptr) {
                    redisFree(context);
                }
                continue;
        }

        auto reply = (redisReply*)redisCommand(context,"AUTH %s",pwd_.c_str());
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
    //这里应该涉及移动构造
    check_thread_ = std::thread([this](){
        while(!b_stop_){
            counter_++;
            if(counter_ == 60){
                CheckRedisConnectValid();
                counter_ = 0;
            }
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    });
}   
RedisConPool::~RedisConPool(){
    // std::lock_guard<std::mutex> guard_lock(mutex_);
    Close();
    std::lock_guard<std::mutex> guard_lock(mutex_);
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
    check_thread_.join();
}
//析构函数可能死锁--------------------
/*
时间  主线程（析构）                    后台线程（CheckRedisConnectValid）
───  ──────────────────────────────    ─────────────────────────────────
T1                                       lock_guard(mutex_)  → 拿到锁
T2                                       读取 pool_size
T3                                       释放锁
T4                                       PING 连接 C1...（耗时操作）
T5    lock_guard(mutex_) → 拿到锁 ✅
T6    Close()
T7      b_stop_ = true
T8      check_thread_.join()  ← 阻塞！
                                       PING 结束
T9                                      ReturnRedisConnection()
T10                                       lock_guard(mutex_) → ❌ 拿不到锁！
                                          因为主线程在 T5 已经持有了
*/

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
void RedisConPool::CheckRedisConnectValid(){
    std::size_t pool_size;
    {
        std::lock_guard<std::mutex> guard_lock(mutex_);
        pool_size = connections_.size();
    }
    for(std::size_t i = 0; i < pool_size; i++){
        /*  这里不能用GetRedisConnection() 它是阻塞的
        --1次要问题：池空时检查线程会阻塞 因为需要cond_.wait()这里会阻塞，这里绝对是不允许的
        --2主要问题： 如果检查线程用 GetRedisConnection()（阻塞版本）
            时间  检查线程                         业务线程（处理用户请求）
            ───  ────────────────────────────────  ────────────────────────────
            T1   取出 C1（池中只剩 [C2]）
            T2   PING C1...
            T3                                    用户请求来了，需要 Redis 连接
            T4   放回 C1（池中 [C2, C1]）
            T5                                    取出 C1（池中只剩 [C2]）
            T6   取出 C2（池中空 []）
            T7   PING C2...
            T8                                    用户请求又来了
            T9   放回 C2（池中 [C2]）
            T10                                   取出 C2（池中空 []）
            T11  开始第二轮遍历
                想取 C1 → 池空 → 阻塞等待 😱
            ！！！检查线程本来只想检查连接，但现在被迫等待业务线程还连接才能继续。如果业务线程一直有请求（比如高并发），
                检查线程一直抢不到连接，就一直无法完成检查
        因此需要使用一个非阻塞的来获取connect
        */
        // auto connect = GetRedisConnection();
        auto connect = GetConNonBlock();
        //检验是否有效 
        //这里处理了GetConNonBlock()返回nullptr的情况
        if(connect == nullptr){
            LOGE << "GetConNonBlock() error" << std::endl;
            continue;
        }
        try{
            auto reply = (redisReply*)redisCommand(connect.get(),"PING");
            //先看底层 I/O／协议层有没有错
            if(connect->err){
                LOGI << "Connection error: " << connect->err << std::endl;
                if(reply){
                    freeReplyObject(reply);
                }   
                //  这段写的不错！！！！！！！！
                connect.reset();
                fail_count_++;
                continue;
            }
            //再看 Redis 自身返回的是不是 ERROR
            if (!reply || reply->type == REDIS_REPLY_ERROR){
                LOGI << "reply is null, redis ping failed: " << std::endl;
                if(reply){
                    freeReplyObject(reply);
                }
                connect.reset();
                fail_count_++;
                continue;
            }
            //这里没问题了，直接还回去即可
            ReturnRedisConnection(std::move(connect));            
        }catch(std::exception& e){
            LOGE << "Exception is :" << e.what() << std::endl;
            connect.reset();
            fail_count_++;
            continue;
        }
        //执行重连逻辑
        while (fail_count_ > 0) {
            bool res = Reconnect();
            if(res){
                fail_count_--;
            }
            else {
                //留给下次再重试
                break;
            }
        }
    }
}

bool RedisConPool::Reconnect(){
    //这里进行连接
    auto raw = redisConnect(host_.c_str(),port_);
    if(raw == nullptr || raw->err != 0){
        redisFree(raw);
        LOGE << "Reconnect() err! err is : redisConnect(host,port) err" << std::endl;
        return false;
    }
    //这里进行认证
    auto reply = (redisReply*)redisCommand(raw,"AUTH %s",pwd_.c_str());
    if(reply == nullptr || reply->type == REDIS_REPLY_ERROR){
        if(reply){
            freeReplyObject(reply);
        }
        LOGI << "[重连]认证失败" << std::endl;
        redisFree(raw);
        return false;
    }
    
    LOGI << "重连成功" << std::endl;
    //raw 是一个原始指针（redisContext*），原始指针是平凡可复制的值类型
    // auto connect = RedisContextPtr(std::move(raw));
    auto connect = RedisContextPtr(raw);
    ReturnRedisConnection(std::move(connect));
    return true;
}   

RedisContextPtr RedisConPool::GetConNonBlock(){
    std::lock_guard<std::mutex> guard_lock(mutex_);
    if(b_stop_ || connections_.empty()){
        return nullptr;
    }
    auto connect = std::move(connections_.front());
    connections_.pop();
    return connect;
}