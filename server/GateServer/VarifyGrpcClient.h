#pragma once
#include <grpcpp/grpcpp.h>
#include "message.grpc.pb.h"
#include "const.h"
#include "Singleton.h"
#include <atomic>
#include <queue>
#include <condition_variable>

using grpc::Channel;
using grpc::Status;
using grpc::ClientContext;

using message::GetVarifyReq;
using message::GetVarifyRsp;
using message::VarifyService;

//其实这里VarifyConPool可以设为单例，只不过，Singleton的GetInstance必须支持可变参数
class VarifyConPool{
public:
    VarifyConPool(size_t poolSize, std::string host, std::string port);
    ~VarifyConPool();
    void Close();
    std::unique_ptr<VarifyService::Stub> getConnection();
    void returnConnection(std::unique_ptr<VarifyService::Stub> context);
    //流程：--------
    // 线程A（执行析构）          // 线程B/C/D（在 getConnection 中等待）
    // ~VarifyConPool() {
    //     lock(mutex_);              ① 线程A获取锁
    //     Close();                   ② notify_all() 唤醒线程B/C/D
    //     // ...                     ③ 线程B/C/D尝试获取锁 → 失败，阻塞等待
    //     while(...) pop();          ④ 线程A继续执行，清空队列
    // }                              ⑤ 线程A离开作用域，锁释放
    //                                ⑥ 线程B抢到锁，从 wait() 返回
    //                                ⑦ 线程B检测 b_stop_ == true，返回 nullptr
    //                                ⑧ 线程C抢到锁，重复...
private:
        //std::atomic<bool> 是 C++ 标准库提供的一种原子布尔类型，用于在多线程环境下安全地读写布尔值，而不需要额外的锁。
        std::atomic<bool> b_stop_;
        std::size_t poolSize_;
        std::string host_;
        std::string port_;
        /*    连接池    的核心语义是FIFO(先进先出)——先放入的空闲连接，先被取出使用。std::queue 天然就是 FIFO 数据结构
            其实也可以用vector来模拟FIFO
            connections_.push_back(stub);    
            connections_.erase(connections_.begin());*/
        std::queue<std::unique_ptr<VarifyService::Stub>> connections_;
        std::mutex mutex_;
        std::condition_variable cond_;
};

class VarifyGrpcClient: public Singleton<VarifyGrpcClient>{
    friend class Singleton<VarifyGrpcClient>;
public:
    //我都想法是：1.GateServer收到client的email 然后发送给VarifyGrpcClient  2.VarifyGrpcClient 使用grpc获取一个验证码服务器的验证码?????????????????????????//
    GetVarifyRsp GetVarifyCode(std::string email);
private:
    VarifyGrpcClient();
    std::unique_ptr<VarifyConPool> pool_;
};
