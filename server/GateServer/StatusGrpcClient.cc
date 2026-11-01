#include "StatusGrpcClient.h"
#include "ConfigMgr.h"


StatusConPool::StatusConPool(std::size_t poolSize,std::string host,std::string port)
    :poolSize_(poolSize),host_(host),port_(port),b_stop_(false){
    for(std::size_t i = 0; i < poolSize; i++){
        std::shared_ptr<Channel> channel = grpc::CreateChannel(host + ":" + port,grpc::InsecureChannelCredentials());
        connections_.push(StatusService::NewStub(channel));
    }
}

StatusConPool::~StatusConPool(){
    Close();
    std::lock_guard<std::mutex> guard_lock(mutex_);
    while(!connections_.empty()){
        connections_.pop();
    }
}

std::unique_ptr<StatusService::Stub> StatusConPool::getConnection(){
    std::unique_lock<std::mutex> unique_lock(mutex_);
    cond_.wait(unique_lock,[this](){
        if(b_stop_)
            return true;
        return !connections_.empty();
    });
    if(b_stop_)
        return nullptr;
    auto getStub = std::move(connections_.front());
    connections_.pop();
    return getStub; 
}

void StatusConPool::returnConnection(std::unique_ptr<StatusService::Stub> stub){
    std::lock_guard<std::mutex> guard_lock(mutex_);
    if(b_stop_){
        return;
    }
    connections_.push(std::move(stub));
    cond_.notify_one();
}

void StatusConPool::Close(){
    b_stop_ = true;
    cond_.notify_all();
}


StatusGrpcClient::StatusGrpcClient(){
    auto& cfMgr = ConfigMgr::GetInstance();
    std::string host = cfMgr["StatusServer"]["Host"];
    std::string port = cfMgr["StatusServer"]["Port"];
    statusPool_ = std::make_unique<StatusConPool>(5,host,port);
}

StatusGrpcClient::~StatusGrpcClient(){

}

GetChatServerRsp StatusGrpcClient::GetChatServer(int uid){
    ClientContext context;
    GetChatServerReq request;
    GetChatServerRsp reply;
    request.set_uid(uid);
    auto stub = statusPool_->getConnection();
    Defer defer([this,&stub](){
        statusPool_->returnConnection(std::move(stub));
    });
    Status status = stub->GetChatServer(&context,request,&reply);
    if(status.ok()){
        return reply;
    }else{
        reply.set_error(ErrorCodes::RPCStatusFailed);
        return reply;
    }
}
