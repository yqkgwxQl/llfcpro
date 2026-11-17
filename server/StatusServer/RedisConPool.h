#pragma once
#include <atomic>
#include <string>
#include <queue>
#include <hiredis/hiredis.h>
#include <condition_variable>
#include <mutex>
#include <thread>
#include <atomic>
#include <chrono>
#include <sys/time.h>

/*为什么 RedisConPool 不用 gRPC？
因为 Redis 服务器根本不认识 gRPC。Redis 讲的是自己的语言——RESP 协议（REdis Serialization Protocol）：
客户端发送：  *3\r\n$3\r\nSET\r\n$3\r\nkey\r\n$5\r\nvalue\r\n
Redis 回复：  +OK\r\n
你只能用 hiredis（或者别的 Redis 客户端）去跟它对话，不能用 gRPC。*/

/*自定义删除器  ——>  why?  ——>  redisContext 是 C 语言库分配的资源，释放它需要调用 redisFree()，而不是 C++ 的 delete。因此不能直接用 
std::unique_ptr<redisContext>，必须提供一个自定义删除器*/

struct RedisContextDeleter{
    void operator()(redisContext* ctx) const{
        if(ctx != nullptr)
            redisFree(ctx);
    }
};
//使用别名化简
using RedisContextPtr = std::unique_ptr<redisContext,RedisContextDeleter>;
class RedisConPool{
public:
    RedisConPool(const char* host,const int port,const std::size_t poolSize,const char* pwd);
    ~RedisConPool();
    void Close();
    void ClearConnections();
    // RedisContextPtr GetRedisConnection();
    //带超时检测的版本，防止GetRedisConnection()由于redis没有启动或中断等情况，导致GetRedisConnection()函数卡住
    RedisContextPtr GetRedisConnection(std::chrono::milliseconds timeout = std::chrono::seconds(3));
    void CheckRedisConnectValid();
    void ReturnRedisConnection(RedisContextPtr);
    bool Reconnect();
    RedisContextPtr GetConNonBlock();
    void DiscardConnection(RedisContextPtr bad);    // 销毁 + 补一条
    bool EnsureCapacity(); 
    void SubBorrowed();
private:
    std::string host_;
    int port_;
    std::size_t poolSize_;
    std::atomic<bool> b_stop_;
    std::queue<RedisContextPtr> connections_;
    std::mutex mutex_;
    std::condition_variable cond_;
    //这里只是提供redis连接池，但是缺少redis连接的维护，必须定时检查redis连接的可靠性，然后更新redis连接
    std::thread check_thread_;
    int counter_;   //计数器，60秒触发检查
    std::atomic<int> fail_count_;  //记本轮检查失败数，用于重连
    std::string pwd_;
    std::size_t borrowed_;
    struct timeval tv_;   //redisConnect的超时时间
};


class RedisConnGuard{
public:
    RedisConnGuard(RedisConPool* pool,RedisContextPtr con);
    ~RedisConnGuard();
    RedisConnGuard(const RedisConnGuard&) = delete;
    RedisConnGuard& operator=(const RedisConnGuard&) = delete;
    void markBroken();
    redisContext* get();
private:
    RedisConPool* pool_;
    RedisContextPtr con_;
    bool broken_;
};

