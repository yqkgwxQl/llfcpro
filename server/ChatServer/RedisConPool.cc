#include "RedisConPool.h"
#include "Logger.h"
#include <iostream>
#include <chrono>

//if redis一开始没启动，也不会报错。只不过连接池里全为空，therefore要对GetRedisConnection()进行防御
RedisConPool::RedisConPool(const char* host,const int port,const std::size_t poolSize,const char* pwd)
    :host_(host), port_(port), poolSize_(poolSize), b_stop_(false), counter_(0), fail_count_(0), pwd_(pwd), borrowed_(0), tv_({2, 0}){
        for(std::size_t i = 0; i < poolSize_; i++){
            auto context = redisConnectWithTimeout(host_.c_str(), port_, tv_);
            if(context == nullptr || context->err != 0){
                if (context != nullptr)
                    redisFree(context);
                LOGE << "创建redis连接失败"; 
                continue;
            }

            RedisContextPtr ptr(context);
            auto reply = (redisReply*)redisCommand(ptr.get(),"AUTH %s",pwd_.c_str());
            if (reply == nullptr) {
                LOGE << "补连接失败：AUTH 无响应";
                continue;
            }
            if (reply->type == REDIS_REPLY_ERROR) {
                LOGE << "补连接失败：AUTH 被拒绝（检查 [Redis] Passwd）";
                freeReplyObject(reply);
                continue;
            }

            //执行成功释放redisCommand执行后返回的redisReply所占用的内存
            freeReplyObject(reply);
            LOGI <<"认证成功"<< std::endl;
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

// RedisContextPtr RedisConPool::GetRedisConnection(){
//     std::unique_lock<std::mutex> unique_lock(mutex_);
//     cond_.wait(unique_lock,[this](){
//         if(b_stop_)
//             return true;
//         return !connections_.empty();
//     });
//     if(b_stop_)
//         return nullptr;
//     auto context = std::move(connections_.front());
//     connections_.pop();
//     return context;
// }

RedisContextPtr RedisConPool::GetRedisConnection(std::chrono::milliseconds timeout){
    std::unique_lock<std::mutex> unique_lock(mutex_);
    const bool ready = cond_.wait_for(unique_lock, timeout, [this](){
        if(b_stop_)
            return true;
        return !connections_.empty();
    });
    // cond_.wait_for(unique_lock, timeout, lambda)相较cond_.wait(unique_lock, lambda)加了超时检查——————先执行lambda，为真立即返回。否则则最多等timeout
    if(!ready){
        unique_lock.unlock();   //LOGE 写时也需要锁，防止等待时间过长导致一直持有锁而不进行归还
        LOGE << "Redis 连接池等待超时 " << timeout.count()
        << "ms（空闲连接=0，请检查 Redis 是否启动、[Redis] 地址/密码是否正确）";
        return nullptr;
    }
    if(b_stop_)
        return nullptr;
    auto context = std::move(connections_.front());
    connections_.pop();
    borrowed_++;
    return context;
}
void RedisConPool::ReturnRedisConnection(RedisContextPtr ptr){
    {
        std::lock_guard<std::mutex> guard_lock(mutex_);
        if(b_stop_)
            return;
        // if(borrowed_ > 0)    不能这样写，因为可能你废弃一条连接，但是新连接没有正常建立 ————> 借出数不会-1
        //     borrowed_--;
        if(borrowed_ + connections_.size() >= poolSize_){
            LOGI << "Redis池已满，此条连接废弃，不进行归还";
            return;
        }
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
    EnsureCapacity();
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
            LOGE << "GetConNonBlock() error, continue" << std::endl;
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

// 所谓的重连就是归还新的 ————> 移除旧的，在Reconnect()之前就做过了 
bool RedisConPool::Reconnect(){
    //这里进行连接
    auto context = redisConnectWithTimeout(host_.c_str(), port_, tv_);
    if(context == nullptr || context->err != 0){
        if(context != nullptr)
            redisFree(context);
        LOGE << "Reconnect() err! err is : redisConnect(host,port) err" << std::endl;
        return false;
    }
    RedisContextPtr ptr(context);
    //这里进行认证
    auto reply = (redisReply*)redisCommand(ptr.get(),"AUTH %s",pwd_.c_str());
    if (reply == nullptr) {
        LOGE << "补连接失败：AUTH 无响应";
        return false;
    }
    if (reply->type == REDIS_REPLY_ERROR) {
        LOGE << "补连接失败：AUTH 被拒绝（检查 [Redis] Passwd）";
        freeReplyObject(reply);
        return false;
    }
    
    LOGI << "重连成功" << std::endl;
    //raw 是一个原始指针（redisContext*），原始指针是平凡可复制的值类型
    // auto connect = RedisContextPtr(std::move(raw));
    ReturnRedisConnection(std::move(ptr));
    return true;
}   

void RedisConPool::DiscardConnection(RedisContextPtr bad){
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if(borrowed_ > 0)
            borrowed_--;
    }
    bad.reset();
    // 创建连接
    //con 是裸指针，作用域退出时销毁的只是“这个 8 字节的指针变量”，它所指的 redisContext 对象留在堆上，没人释放。所以要对con进行托管
    auto con = redisConnectWithTimeout(host_.c_str(), port_, tv_);
    if(con == nullptr || con->err != 0){
        if(con != nullptr)
            redisFree(con);
        LOGE << "创建redis连接失败"; 
        return;     // 交给EnsureCapacity去补
    }
    RedisContextPtr ptr(con);
    // 认证连接
    auto reply = (redisReply*)redisCommand(ptr.get(), "AUTH %s", pwd_.c_str());
    if (reply == nullptr) {
        LOGE << "补连接失败：AUTH 无响应";
        return;
    }
    if (reply->type == REDIS_REPLY_ERROR) {
        LOGE << "补连接失败：AUTH 被拒绝（检查 [Redis] Passwd）";
        freeReplyObject(reply);
        return;
    }
    freeReplyObject(reply);
    LOGI << "认证成功";
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if(b_stop_)
            return;
        if(borrowed_ + connections_.size() >= poolSize_)
            return;
        connections_.push(std::move(ptr));
    }
    cond_.notify_one();
}

bool RedisConPool::EnsureCapacity(){
    std::size_t total = 0;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        total = borrowed_ + connections_.size();
    }
    for(std::size_t i = total; i < static_cast<std::size_t>(poolSize_); i++){
        // 创建连接
        auto con = redisConnectWithTimeout(host_.c_str(), port_, tv_);
        if(con == nullptr || con->err != 0){
            if(con != nullptr)
                redisFree(con); 
            LOGE << "补连接失败：redisConnect 无响应（本轮停止扩容）";
            return false;
        }
        RedisContextPtr ptr(con);
        // 认证连接
        auto reply = (redisReply*)redisCommand(ptr.get(), "AUTH %s", pwd_.c_str());
        if (reply == nullptr) {
            LOGE << "补连接失败：AUTH 无响应";
            return false;
        }
        if (reply->type == REDIS_REPLY_ERROR) {
            LOGE << "补连接失败：AUTH 被拒绝（检查 [Redis] Passwd）";
            freeReplyObject(reply);
            return false;
        }
        freeReplyObject(reply);
        LOGI << "认证成功";
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if(b_stop_)
                return false;
            if(borrowed_ + connections_.size() >= poolSize_)
                return true;        //既然容量已够了，已经确定数量对了，不用继续添加新的链接了         
            connections_.push(std::move(ptr));      //ReturnRedisConnection是可以调的，只不过要挪到锁外调。我觉得如果挪到锁外，相当于用了两次锁。所以干脆不调ReturnRedisConnection，只用一次锁
        }
        cond_.notify_one();
    }   
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
      
void RedisConPool::SubBorrowed(){
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if(borrowed_ > 0)
            borrowed_--;
    }
}

RedisConnGuard::RedisConnGuard(RedisConPool* pool,RedisContextPtr con)
    :pool_(pool), con_(std::move(con)), broken_(false){

}

RedisConnGuard::~RedisConnGuard(){
    if(!con_) return;
    if(broken_){
        pool_->DiscardConnection(std::move(con_));
        return;
    }
    pool_->SubBorrowed();
    pool_->ReturnRedisConnection(std::move(con_));
}


void RedisConnGuard::markBroken(){
    broken_ = true;
}

redisContext* RedisConnGuard::get(){
    return con_.get();
}
