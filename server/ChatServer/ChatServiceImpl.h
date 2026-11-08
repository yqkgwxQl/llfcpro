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

using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::Status;
using message::AddFriendReq;
using message::AddFriendRsp;

using message::AuthFriendReq;
using message::AuthFriendRsp;

using message::ChatService;
using message::TextChatMsgReq;
using message::TextChatMsgRsp;
using message::TextChatData;
using message::KickUserReq;
using message::KickUserRsp;

class CServer;
class UserInfo;
class RedisMgr;
class MysqlMgr;
class ChatServiceImpl final: public ChatService::Service{

public:
    /*ChatServiceImpl(std::shared_ptr<CServer> server);   
    我原先想要构造函数直接初始化这个服务器，但是E:\zhongzhuan2\llfcchatpro\server\ChatServer1\ChatServer.cc : 22 时还没有服务器，所以
    有两种方案：1.由于rpc服务器和ChatServer是运行在同一个进程的两个不同的线程上，所以可以将E:\zhongzhuan2\llfcchatpro\server\ChatServer1\ChatServer.cc
    顺序调一下，先创建ChatServer，再创建ChatServiceImpl服务（绑定ChatServer)
        2.还是原来的 ———— 先创建ChatServiceImpl服务再创建ChatServer,然后后续让ChatServerImpl的_p_server进行初始化，这里采用的是方案2 */
    ChatServiceImpl();
    void SetServer(std::shared_ptr<CServer> server);
    Status NotifyAddFriend(ServerContext* context, const AddFriendReq* request,AddFriendRsp* response) override;
    Status NotifyAuthFriend(ServerContext* context,const AuthFriendReq* request, AuthFriendRsp* response) override;
    Status NotifyTextChatMsg(::grpc::ServerContext* context,const TextChatMsgReq* request, TextChatMsgRsp* response) override;
    bool GetBaseInfo(std::string base_key, int uid, std::shared_ptr<UserInfo>& userinfo);
private:
    std::shared_ptr<CServer> _p_server;
};
