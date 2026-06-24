#pragma once
#include "message.grpc.pb.h"
#include "message.pb.h"
#include <grpcpp/grpcpp.h>
#include <mutex>
#include <map>

using grpc::Status;
using grpc::Server;
using grpc::ServerContext;
using grpc::ServerBuilder;
using message::GetChatServerReq;
using message::GetChatServerRsp;
using message::StatusService;

class ChatServer{
public:
    ChatServer();
    ChatServer(const std::string& host,const std::string& port,const std::string& name);
    ~ChatServer();
    ChatServer(const ChatServer& cs);
    ChatServer& operator =(const ChatServer& cs);
// private:
    std::string _host;
    std::string _port;
    std::string _name;
    int _con_count;
};
/* final关键字的作用
语法上	禁止该类被继承（类似 Java 的 final class）
优化上	允许编译器去虚化，把虚函数调用变成普通调用，略微提升性能*/
//状态服务器实际上只是返回给客户端载荷较低的聊天服务器的ip,port,server_name,token(用于验证)???????????验证什么？
class StatusServiceImpl final: public StatusService::Service{
public:
    StatusServiceImpl();
    ~StatusServiceImpl();
    // rpc GetChatServer (GetChatServerReq) returns (GetChatServerRsp) {}
    virtual ::grpc::Status GetChatServer(::grpc::ServerContext* context, const ::message::GetChatServerReq* request, 
        ::message::GetChatServerRsp* response);
    // rpc Login (LoginReq) returns (LoginRsp) {};
    virtual ::grpc::Status Login(::grpc::ClientContext* context, const ::message::LoginReq& request,
         ::message::LoginRsp* response);
private:
    ChatServer getChatServer();
    std::unordered_map<std::string,ChatServer> _servers;
    // std::map<std::string,ChatServer> _servers;
    void insertToken(int uid,std::string token);
    std::mutex _server_mtx;
};