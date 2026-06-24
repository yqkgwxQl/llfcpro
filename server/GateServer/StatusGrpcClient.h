#pragma once
#include "const.h"
#include "Singleton.h"
#include "ConfigMgr.h"
#include <grpcpp/grpcpp.h> 
#include "message.grpc.pb.h"
#include "message.pb.h"
#include <queue>
#include <string>
#include <condition_variable>
#include <mutex>
#include <atomic>

using grpc::Channel;
using grpc::Status;
using grpc::ClientContext;
using message::GetChatServerReq;
using message::GetChatServerRsp;
using message::StatusService;


class StatusConPool{
public:
    StatusConPool(std::size_t poolSize,std::string host,std::string port);
    ~StatusConPool();
    std::unique_ptr<StatusService::Stub> getConnection();
    void returnConnection(std::unique_ptr<StatusService::Stub> stub);
    void Close();
private:
    std::size_t poolSize_;
    std::string host_;
    std::string port_;
    std::atomic<bool> b_stop_;
    std::queue<std::unique_ptr<StatusService::Stub>> connections_;
    std::condition_variable cond_;
    std::mutex mutex_;
};

class StatusGrpcClient: public Singleton<StatusGrpcClient>{
    friend class Singleton<StatusGrpcClient>;
public:
    ~StatusGrpcClient();
    GetChatServerRsp GetChatServer(int uid);
private:
    StatusGrpcClient();
    std::unique_ptr<StatusConPool> statusPool_;
};

