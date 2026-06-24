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
using message::LoginReq;
using message::LoginRsp;
using message::StatusService;

class StatusConPool{

public:
    StatusConPool(std::size_t size,std::string host,std::string port);
    ~StatusConPool();
    void Close();
    std::unique_ptr<StatusService::Stub> GetConnection();
    void ReturnConnection(std::unique_ptr<StatusService::Stub>);
private:
    std::size_t _size;
    std::string _host;
    std::string _port;
    std::atomic<bool> _b_stop;
    std::queue<std::unique_ptr<StatusService::Stub>> _connections;
    std::mutex _mutex;
    std::condition_variable _cond;
};
class StatusGrpcClient: public Singleton<StatusGrpcClient>{
    friend class Singleton<StatusGrpcClient>
public:
    ~StatusGrpcClient();
    LoginRsp Login(int uid,std::string& token);
private:
    StatusGrpcClient();
    std::unique_ptr<StatusConPool> _statusPool;
};