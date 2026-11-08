#include "StatusGrpcClient.h"

StatusConPool::StatusConPool(std::size_t size,std::string host,std::string port)
    :_size(size),_host(host),_port(port),_b_stop(false){
    for(std::size_t i = 0; i < _size; i++){
        std::shared_ptr<Channel> channel = grpc::CreateChannel(_host + ":" + _port,grpc::InsecureChannelCredentials());
        // _connections.push(std::move(StatusService::NewStub(channel)));
        _connections.push(StatusService::NewStub(channel));
    }
}

StatusConPool::~StatusConPool(){
    Close();
    std::lock_guard<std::mutex> guard_lock(_mutex);
    while (!_connections.empty()){
        _connections.pop();
    }
}

void StatusConPool::Close(){
    _b_stop = true;
    _cond.notify_all();
}

std::unique_ptr<StatusService::Stub> StatusConPool::GetConnection(){
    std::unique_lock<std::mutex> unique_lock(_mutex);
    _cond.wait(unique_lock,[this](){
        if(_b_stop)
            return true;
        return !_connections.empty();
    });
    if(_b_stop){
        return nullptr;
    }
    auto con = std::move(_connections.front());
    _connections.pop();
    return con;
}

void StatusConPool::ReturnConnection(std::unique_ptr<StatusService::Stub> con){
    {
        std::lock_guard<std::mutex> guard_lock(_mutex);
        if(_b_stop){
            return;
        }
        _connections.push(std::move(con));
    }
    _cond.notify_one();
}
StatusGrpcClient::StatusGrpcClient(){
    auto& cfgMgr = ConfigMgr::GetInstance();
    std::string host = cfgMgr["StatusServer"]["Host"];
    std::string port = cfgMgr["StatusServer"]["Port"];
    _statusPool = std::make_unique<StatusConPool>(5,host,port);
}

StatusGrpcClient::~StatusGrpcClient(){
    
}

LoginRsp StatusGrpcClient::Login(int uid,std::string& token){
    ClientContext context;
    LoginReq request;
    LoginRsp reply;
    request.set_uid(uid);
    request.set_token(token);
    auto stub = _statusPool->GetConnection();
    Defer defer([this,&stub](){
        _statusPool->ReturnConnection(std::move(stub));
    });
    Status status = stub->Login(&context,request,&reply);
    if(status.ok()){
        return reply;
    }else{
        reply.set_error(ErrorCodes::RPCStatusFailed);
        return reply;
    }
}