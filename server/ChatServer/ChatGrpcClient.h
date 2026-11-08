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
#include <unordered_map>

using grpc::Channel;
using grpc::Status;
using grpc::ClientContext;
using message::AddFriendReq;
using message::AddFriendRsp;

using message::AuthFriendReq;
using message::AuthFriendRsp;

using message::GetChatServerRsp;
using message::LoginRsp;
using message::LoginReq;
using message::ChatService;

using message::TextChatMsgReq;
using message::TextChatMsgRsp;
using message::TextChatData;

using message::KickUserReq;
using message::KickUserRsp;

class ChatConPool{

public:
    ChatConPool(std::size_t size,std::string host,std::string port);
    ~ChatConPool();
    void Close();
    std::unique_ptr<ChatService::Stub> GetConnection();
    void ReturnConnection(std::unique_ptr<ChatService::Stub>);
private:
    std::size_t _size;
    std::string _host;
    std::string _port;
    std::atomic<bool> _b_stop;
    std::queue<std::unique_ptr<ChatService::Stub>> _connections;
    std::mutex _mutex;
    std::condition_variable _cond;
};

class UserInfo;
class ChatGrpcClient: public Singleton<ChatGrpcClient>{
    friend class Singleton<ChatGrpcClient>;
public:
    // 通知对方服务器：有人申请加好友
    AddFriendRsp NotifyAddFriend(const std::string&server_name,AddFriendReq req);
    //通知对方服务器：好友申请已通过
    AuthFriendRsp NotifyAuthFriend(std::string& server_name, const AuthFriendReq& req);
    //跨服务器获取用户基本信息
    bool GetBaseInfo(std::string base_key, int uid, std::shared_ptr<UserInfo>& userinfo);
    // 转发聊天消息到对方服务器
    TextChatMsgRsp NotifyTextChatMsg(std::string server_name, const TextChatMsgReq& req, const Json::Value& rtvalue); //？？？我感觉rtvalue没有任何作用
    ~ChatGrpcClient();
private:
    ChatGrpcClient();
    std::unordered_map<std::string, std::unique_ptr<ChatConPool>> _pools;
};
