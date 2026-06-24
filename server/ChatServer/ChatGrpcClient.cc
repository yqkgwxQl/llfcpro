#include "ChatGrpcClient.h"
#include "data.h"

ChatConPool::ChatConPool(std::size_t size,std::string host,std::string port)
    :_size(size),_host(host),_port(port),_b_stop(false){
    for(std::size_t i = 0; i < _size; i++){
        std::shared_ptr<Channel> channel = grpc::CreateChannel(_host + ":" + _port,grpc::InsecureChannelCredentials());
        // _connections.push(std::move(StatusService::NewStub(channel)));
        _connections.push(ChatService::NewStub(channel));
    }
}

ChatConPool::~ChatConPool(){
    Close();
    std::lock_guard<std::mutex> guard_lock(_mutex);
    while (!_connections.empty()){
        _connections.pop();
    }
}

void ChatConPool::Close(){
    _b_stop = true;
    _cond.notify_all();
}

std::unique_ptr<ChatService::Stub> ChatConPool::GetConnection(){
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

void ChatConPool::ReturnConnection(std::unique_ptr<ChatService::Stub> con){
    {
        std::lock_guard<std::mutex> guard_lock(_mutex);
        if(_b_stop){
            return;
        }
        _connections.push(std::move(con));
    }
    _cond.notify_one();
}

ChatGrpcClient::ChatGrpcClient(){
    auto& cfg = ConfigMgr::GetInstance();
    std::vector<std::string> words;
    std::string word;
    std::string server_list = cfg["PeerServer"]["Servers"];
    std::stringstream ss(server_list);
    while(std::getline(ss,word,',')){
        words.push_back(word);
    }
    for(auto& server_name: words){
        std::string host = cfg[server_name]["Host"];
        std::string port = cfg[server_name]["Port"];
        _pools[server_name] = std::make_unique<ChatConPool>(5,host,port);
    }
}

ChatGrpcClient::~ChatGrpcClient(){
    
}

AddFriendRsp ChatGrpcClient::NotifyAddFriend(const std::string&server_name,AddFriendReq req){
    ClientContext context;
    AddFriendRsp rsp;
    auto iter = _pools.find(server_name);
    if(iter == _pools.end()){
        rsp.set_error(ErrorCodes::UidInvalid);
        return rsp;
    }
    auto pool = std::move(iter->second);
    auto stub = pool->GetConnection();
    Defer defer([this,&pool,&stub](){
        pool->ReturnConnection(std::move(stub));
    });
    Status state = stub->NotifyAddFriend(&context,req,&rsp);
    if(state.ok()){
        return rsp;
    }else{
		rsp.set_error(ErrorCodes::RPCFailed);
        return rsp;
    }
}

AuthFriendRsp ChatGrpcClient::NotifyAuthFriend(std::string server_ip, const AuthFriendReq& req) {
    AuthFriendRsp rsp;
    return rsp;
}

bool ChatGrpcClient::GetBaseInfo(std::string base_key, int uid, std::shared_ptr<UserInfo>& userinfo) {
    return true;
}

TextChatMsgRsp ChatGrpcClient::NotifyTextChatMsg(std::string server_ip, 
    const TextChatMsgReq& req, const Json::Value& rtvalue) {

    TextChatMsgRsp rsp;
    return rsp;
}