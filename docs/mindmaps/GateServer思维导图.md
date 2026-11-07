---

excalidraw-plugin: parsed
tags: [excalidraw]

---
==⚠  Switch to EXCALIDRAW VIEW in the MORE OPTIONS menu of this document. ⚠== You can decompress Drawing data with the command palette: 'Decompress current Excalidraw file'. For more info check in plugin settings under 'Saving'


# Excalidraw Data

## Text Elements
syntax = "proto3";

package message;

service VarifyService {
  rpc GetVarifyCode (GetVarifyReq) returns (GetVarifyRsp) {}
}

message GetVarifyReq {
  string email = 1;
}

message GetVarifyRsp {
  int32 error = 1;
  string email = 2;
  string code = 3;
}
 ^jk6p2WNa

massage.proto ^BkvWrQQv

# 生成消息类代码
protoc --cpp_out=. message.proto ^U9ZoFtVZ

消息类代码（.pb.h / .pb.cc）：对应 .proto 中的 message。生成了请求和响应对象（如 GetVarifyReq、GetVarifyRsp），包含字段的 getter/setter 以及序列化/反序列化方法。 ^SPwKoJIh

# 生成 gRPC 服务代码
protoc --grpc_out=. --plugin=protoc-gen-grpc=`which grpc_cpp_plugin` message.proto ^BsBUs5Yj

rpc:远程服务调用 ^HR7pPS8m

服务类代码（.grpc.pb.h / .grpc.pb.cc）：对应 .proto 中的 service。生成了客户端的代理类 Stub 和服务端的抽象基类 Service，并提供了创建 Stub 的工厂方法 NewStub() ^ZQCsBfeP

服务端需求：
框架接收请求 → 反序列化 → 【这里需要你的业务代码】 → 序列化 → 发送响应
客户端需求：
你调用 stub_->GetVarifyCode() → 框架帮你搞定一切 → 返回结果 ^JbTn1ZDK

对应不同的需求，由此有不同的解决办法 ^MvsM87UN

服务端：继承 + 虚函数
客户端：使用代理类直接发送请求即可 ^iWOsJ72E

Stub（伪客户端/代理）：封装了网络通信部分，让客户端只需像调用本地函数一样发起调用，无需关注底层网络细节。 ^rU9qSqzq

Service（伪服务端/骨架）：同样封装了网络部分，开发者只需继承它并重写虚函数（即业务逻辑），真正的服务端就搭建完成了。 ^VixP1wJv

int main(){
   1.读取配置 port
    try{    
    2.设置中断信号
    3.启动服务器
        std::make_shared<CServer>(ioc,gate_port)->Start();;
        ioc.run();
} ^yzAHMr00

GateServer ^dsDIH2Y7

void CServer::Start(){
    1.启动ioService池来获取一个io_context
    auto& io_context = AsioIOServicePool::GetInstance()->GetIOService();
    2.用获取到的io_context来构造一个HttpConnection连接
    std::shared_ptr<HttpConnection> new_con = std::make_shared<HttpConnection>(io_context);
    3.这里Start会很快回来，因为new_con->Start()是异步的
    _acceptor.async_accept(new_con->GetSocket(),[new_con,self](beast::error_code ec){
            new_con->Start();
            self->Start();
    });
} ^AHkMXJ9t

CServer ^t0N87mjt

HttpConnection ^Ab6ITHIw


void HttpConnection::Start(){
    1.只是负责读取一个请求，然后返回一个响应。
   http::async_read(_socket,_buffer,_request,[self] (beast::error_code ec,std::size_t bytes_transferred){
            boost::ignore_unused(bytes_transferred); 
            self->CheckDeadline();
            self->HandleReqest();
    });
}
 ^ymklfVqf

  -------其实就是根据对应的请求，填写reply.body,跳转到发送响应的逻辑
void HttpConnection::HandleReqest(){
    _response.version(_request.version());
    //这里设置为短链接
    _response.keep_alive(false);

    //method 请求方式
    if(_request.method() == http::verb::get){
        PreParseGetParam();
        1.设置reply.body
        bool success = LogicSystem::GetInstance()->HandleGet(_get_url,shared_from_this());
        2.设置reply.状态码和状态码描述
        _response.result(http::status::ok);
        _response.set(http::field::server,"GateSever");
        3.跳转到发送响应
        WriteResponse();
        return;
    }

    if(_request.method() == http::verb::post){
        //这里不用再PreParseGetParam(); 因为post请求是不包含路径的
        1.设置reply.body
        bool success = LogicSystem::GetInstance()->HandlePost(_request.target(),shared_from_this());
        2.设置reply.状态码和状态码描述
       _response.result(http::status::ok);
        _response.set(http::field::server,"GateServer");
        3.跳转到发送响应
        WriteResponse();
        return;
    }
} ^qRuDbHzD


//这里只是在response的报头里设置content_length ，然后发送response，设置回调(服务器断开连接)
void HttpConnection::WriteResponse(){
    1.加上长度，发送响应
    _response.content_length(_response.body().size());
    http::async_write(_socket,_response});
} ^MOF8XX5B

1.这里会有AsioIOServicePool提供多个io_context
2.CServer服务器启动，为每个客户端所向服务器的连接，提供一个连接
（这里是不会阻塞的，服务器很快就可以向一个客户端提供连接，进而等待下一个客户端的连接） ^9hCeYkNC

对应的业务逻辑（每个连接都是一致的） ^cYZGN0J5


void LogicSystem::RegGet(std::string url, HttpHandler handler){
    _get_handlers.insert(std::make_pair(url,handler));
}

void LogicSystem::RegPost(std::string url, HttpHandler handler){
    _post_handlers.insert(std::make_pair(url,handler));
}


LogicSystem::LogicSystem(){
    -------------- 1.在_get_handlers里添加 /get_test 网址的回调
    RegGet("/get_test",[](std::shared_ptr<HttpConnection> connection){
                connection->_response.set(http::field::content_type, "text/plain");
 ----------------1.1 填写reply.body
         beast::ostream(connection->_response.body()) << "receiver get_test req";
        int i = 0;
        for (auto& elem : connection->_get_params) {
            i++;
            beast::ostream(connection->_response.body()) << "param" << i << " key is " << elem.first;
            beast::ostream(connection->_response.body()) << ", " <<  " value is " << elem.second << std::endl;
        }

    });
    -------------- 2.在_post_handlers里添加/get_varifycode 网址的回调
    RegPost("/get_varifycode",[](std::shared_ptr<HttpConnection> connection){
        auto body_str = boost::beast::buffers_to_string(connection->_request.body().data());
        std::cout << "receive body is " << body_str << std::endl;
        connection->_response.set(http::field::content_type,"text/json");
        Json::Value root;
        Json::Reader reader;
        Json::Value src_root;
        bool parse_success = reader.parse(body_str,src_root);
        auto email = src_root["email"].asString();
   -----------------2.1 回调里: 向request里的email里发送验证码（新建一个VarifyGrpcClient客户端，向node.js写的VarifyServer里发送 需要验证码的请求，
        由node.js的服务端发送验证码给用户邮箱里，返回GetVarifyRsp响应给VarifyGrpcClient客户端 GetVarifyRsp：emaill 和 error错误码)
        GetVarifyRsp rsp = VarifyGrpcClient::GetInstance()->GetVarifyCode(email);
   ----------------2.2 填写reply.body
        std::cout << "email is " << email << std::endl;
        root["error"] = rsp.error();
        root["email"] = src_root["email"]; 
        //这里为什么不root["code"] = src_root["code"]; ——————> 验证码不是通过网关服务器发送的，是node.js的VarifyServer发送的网关服务器不用携带验证码
        std::string jsonstr = root.toStyledString();
        beast::ostream(connection->_response.body()) << jsonstr;
        return true;
    });
    -------------- 3.在_post_handlers里添加/user_registe 网址的回调
    RegPost("/user_register",[](std::shared_ptr<HttpConnection> connection){
        auto body_str = boost::beast::buffers_to_string(connection->_request.body().data());
        std::cout << "receive body is " << body_str << std::endl;
        connection->_response.set(http::field::content_type,"text/json");
        Json::Value root;
        Json::Reader reader;
        Json::Value src_root;
        bool parse_success = reader.parse(body_str,src_root);
        -------------3.1 先查找redis中email对应的验证码是否合理
        std::string varifyCode;
        //防止用一个邮箱收取验证码，用另一个邮箱进行注册
        bool b_get_varify = RedisMgr::GetInstance()->Get(src_root["email"].asString(),varifyCode);
        if(!b_get_varify){
            std::cout << "get varifyCode failed!" << std::endl;
            root["error"] = ErrorCodes::Error_Json;
            std::string jsonstr = root.toStyledString();
            beast::ostream(connection->_response.body()) << jsonstr;
            return true;
        }
        ---------------3.2 防止你提供虚假的邮箱的验证码
        if(varifyCode != src_root["varifycode"].asString()){
            std::cout << "varifyCode invalid!" << std::endl;
            root["error"] = ErrorCodes::Error_Json;
            std::string jsonstr = root.toStyledString();
            beast::ostream(connection->_response.body()) << jsonstr;
            return true;
        }
        ----------------3.3查看数据库中是否有此用户
        ----------------3.4 填写reply.body
        root["error"] = 0;
        root["email"] = src_root["email"];
        root["user"]= src_root["user"].asString();
        root["passwd"] = src_root["passwd"].asString();
        root["confirm"] = src_root["confirm"].asString();
        root["varifycode"] = src_root["varifycode"].asString();
        std::string jsonstr = root.toStyledString();
        beast::ostream(connection->_response.body()) << jsonstr;
        return true;
    });
}
 -------------- 4.在_post_handlers里添加/reset_pwd网址的回调
RegPost("/reset_pwd",[](std::shared_ptr<HttpConnection> connection){
        auto body_str = boost::beast::buffers_to_string(connection->_request.body().data());
        std::cout << "receive body is " << body_str << std::endl;
        connection->_response.set(http::field::content_type,"text/json");
        Json::Value root;
        Json::Reader reader;
        Json::Value src_root;
        bool parse_success = reader.parse(body_str,src_root);
        auto email = src_root["email"].asString();
        auto name = src_root["user"].asString();
        auto pwd = src_root["passwd"].asString();

         ---------------4.1 先查找redis中email对应的验证码是否合理
        std::string varifyCode;
        //防止用一个邮箱收取验证码，用另一个邮箱进行注册 / 验证码过期
        bool b_get_varify = RedisMgr::GetInstance()->Get(CODEPREFIX + src_root["email"].asString(),varifyCode);
        if(!b_get_varify){
            std::cout << "get varifyCode failed!" << std::endl;
            root["error"] = ErrorCodes::VarifyExpired;
            std::string jsonstr = root.toStyledString();
            beast::ostream(connection->_response.body()) << jsonstr;
            return true;
        }
        //防止你提供虚假的邮箱的验证码
        if(varifyCode != src_root["varifycode"].asString()){
            std::cout << "varifyCode invalid!" << std::endl;
            root["error"] = ErrorCodes::VarifyCodeErr;
            std::string jsonstr = root.toStyledString();
            beast::ostream(connection->_response.body()) << jsonstr;
            return true;
        }

        ---------------4.2 查询数据库判断此用户名和email是否已在数据库中 ——> 即是否此用户已存在
        bool b_exist = MysqlMgr::GetInstance()->IsExist(name,email);
        if(!b_exist){
            std::cout << "user not exist , you cannot reset pwd!" << std::endl;
            root["error"] = ErrorCodes::UserNotExist;
            std::string jsonstr = root.toStyledString();
            beast::ostream(connection->_response.body()) << jsonstr;
            return true;
        }
        //查询数据库判断用户名和邮箱是否匹配---------------
        bool email_valid = MysqlMgr::GetInstance()->CheckEmail(name,email);
        if(!email_valid){
            std::cout << " user email not match" << std::endl;
            root["error"] = ErrorCodes::EmailNotMatch;
            std::string jsonstr = root.toStyledString();
            beast::ostream(connection->_response.body()) << jsonstr;
            return true;
        }

        // 更新密码为最新密码
        bool b_up = MysqlMgr::GetInstance()->UpdatePwd(name, pwd);
        if (!b_up) {
            std::cout << " update pwd failed" << std::endl;
            root["error"] = ErrorCodes::PasswdUpFailed;
            std::string jsonstr = root.toStyledString();
            beast::ostream(connection->_response.body()) << jsonstr;
            return true;
        }

        std::cout << "succeed to update password" << pwd << std::endl;
        root["error"] = 0;
        root["email"] = src_root["email"];
        root["user"]= src_root["user"].asString();
        root["passwd"] = src_root["passwd"].asString();
        root["varifycode"] = src_root["varifycode"].asString();
        std::string jsonstr = root.toStyledString();
        beast::ostream(connection->_response.body()) << jsonstr;
        return true;
    });
     -------------- 5.在_post_handlers里添加/user_login网址的回调
    RegPost("/user_login",[](std::shared_ptr<HttpConnection> connection){
        auto body_str = boost::beast::buffers_to_string(connection->_request.body().data());
        std::cout << "receive body is " << body_str << std::endl;
        connection->_response.set(http::field::content_type,"text/json");
        Json::Value root;
        Json::Reader reader;
        Json::Value src_root;
        bool parse_success = reader.parse(body_str,src_root);
        auto email = src_root["email"].asString();
        auto pwd = src_root["passwd"].asString();
        UserInfo userInfo;  

          -------5.1 查询数据库判断此email是否已在数据库中 ——> 即是否此用户已存在
        bool b_exist = MysqlMgr::GetInstance()->IsExist_email(email);
        if(!b_exist){
            std::cout << "email is not exist!" << std::endl;
            root["error"] = ErrorCodes::EmailNotExist;
            std::string jsonstr = root.toStyledString();
            beast::ostream(connection->_response.body()) << jsonstr;
            return true;
        }
       -------5.2  查询数据库判断密码和邮箱是否匹配---------------
        bool pwd_valid = MysqlMgr::GetInstance()->CheckPwd(email,pwd,userInfo);//userInfo 作为输出型参数，回传给用户
        /*回传：userInfo.pwd = ret->getString("pwd");
                userInfo.email = ret->getString("email");
                userInfo.name = ret->getString("name");
                userInfo.uid = ret->getInt("uid");*/
        if(!pwd_valid){
            std::cout << " email and pwd not match" << std::endl;
            root["error"] = ErrorCodes::PasswdInvalid;
            std::string jsonstr = root.toStyledString();
            beast::ostream(connection->_response.body()) << jsonstr;
            return true;
        }

       -------5.3查询StatusServer找到合适的连接
        auto reply = StatusGrpcClient::GetInstance()->GetChatServer(userInfo.uid);
        if (reply.error()) {
            std::cout << " grpc get chat server failed, error is " << reply.error()<< std::endl;
            root["error"] = ErrorCodes::RPCStatusFailed;
            std::string jsonstr = root.toStyledString();
            beast::ostream(connection->_response.body()) << jsonstr;
            return true;
        }

        std::cout << "succeed to load userinfo uid is " << userInfo.uid << std::endl;
        root["error"] = 0;
        root["user"] = userInfo.name;
        root["uid"] = userInfo.uid;
        root["token"] = reply.token();
        root["host"] = reply.host();
        root["port"] = reply.port();
        std::string jsonstr = root.toStyledString();
        beast::ostream(connection->_response.body()) << jsonstr;
        return true;
    });

//这里只是调用一下get的回调函数而已
bool LogicSystem::HandleGet(std::string path, std::shared_ptr<HttpConnection> con){
    if(_get_handlers.find(path) == _get_handlers.end()){
        return false;
    }
    //执行回调，传con作为参数
    _get_handlers[path] (con);
    return true;
}

bool LogicSystem::HandlePost(std::string path, std::shared_ptr<HttpConnection> con){
    if(_post_handlers.find(path) == _post_handlers.end()){
        return false;
    }
    //执行回调，传con作为参数
    _post_handlers[path] (con);
    return true;
}

 ^P4NinCp1

LogicSystem ^k2kCIypz

这是整个的构造函数，构造函数初始化时，会在
_get_handlers和_post_handlers里面注册回调 ^VdCPxPrL

执行回调 ^OXm6msCL

LogicSystem ^ailE11id

---------VarifyGrpcClient的构造函数
VarifyGrpcClient::VarifyGrpcClient(){
    1.从配置文件里提取node.jsVarifyServer的host,port
    ConfigMgr& CfgMgr = ConfigMgr::GetInstance();
    std::string host = CfgMgr["VarifyServer"]["Host"];
    std::string port = CfgMgr["VarifyServer"]["Port"];
   2.创建RPCconnectionPool,里面有5个代理Stub
    pool_ = std::make_unique<RPConPool>(5,host,port);
}

//这个函数是同步的，只是多线程可以分别使用各自的代理来获取验证码，因此调用完此函数后，一定发送了验证码
GetVarifyRsp VarifyGrpcClient::GetVarifyCode(std::string email){
    ClientContext context;
    GetVarifyReq request;
    GetVarifyRsp reply;
    request.set_email(email);
----------申请一个代理（当然会阻塞），然后让这个代理去向node.js服务器申请验证码，然后node.js服务器发送验证码后，
stub->GetVarifyCode(&context,request,&reply)返回，得到执行成功的状态status  ——>  返回代理Stub，供其它需要验证码
服务的线程使用
    auto stub = pool_->getConnection();
    Status status = stub->GetVarifyCode(&context,request,&reply);
    if(status.ok()){
        pool_->returnConnection(std::move(stub));
        return reply;
    }else{
        pool_->returnConnection(std::move(stub));
        reply.set_error(ErrorCodes::RPCFailed);
        return reply;
    }
}
 ^TAG0EnEg

池：
    1. AsioIOServicePool池提供上下文io_contex
    2. RPConnectionPool池提供代理Stub
    3.RedisConPool池提供连接Redis数据库的连接对象，能够使用连接
对象执行eg:查询指令
    4.MysqlPool池提供mysql连接 来注册用户、查询mysql数据库里的数据 ^9WphMBCi

单例：1.AsioIOServicePool
            2.LogicSystem
            3.RedisMgr
            4.VarifyClient
            5.其实RPConnectionPool也应该设置为单例
            6.其实RedisConPool也应该设置为单例 ^mJVtCtQP

RedisMgr ^G8ZCDqGM

class RedisMgr: public Singleton<RedisMgr>, 
    public std::enable_shared_from_this<RedisMgr>
{
    friend class Singleton<RedisMgr>;
public:
    ~RedisMgr();
    // bool Connect(const std::string& host, int port);
    bool Get(const std::string &key, std::string& value);
    bool Set(const std::string &key, const std::string &value);
    // bool Auth(const std::string &password);
    bool LPush(const std::string &key, const std::string &value);
    bool LPop(const std::string &key, std::string& value);
    bool RPush(const std::string& key, const std::string& value);
    bool RPop(const std::string& key, std::string& value);
    bool HSet(const std::string &key, const std::string  &hkey, const std::string &value);
    bool HSet(const char* key, const char* hkey, const char* hvalue, size_t hvaluelen);
    std::string HGet(const std::string &key, const std::string &hkey);
    bool Del(const std::string &key);
    bool ExistsKey(const std::string &key);
    void Close();
private:
    RedisMgr();

    std::unique_ptr<RedisConPool> _con_pool;
}; ^nYUTHg8p

bool RedisMgr::Get(const std::string &key, std::string& value)
{
    auto context = _con_pool->GetRedisConnection();
    if(context == nullptr){
        return false;
    }
    /*using RedisContextPtr = std::unique_ptr<redisContext, RedisContextDeleter>; ——> 模板参数就是 redisContext
     RedisContextPtr.get()返回的不是C风格的原生指针，而是redisContext*  */
    //redisCommand ————> 向 Redis 服务器发送一条命令，并返回执行结果redisReply*
    auto reply = (redisReply*)redisCommand(context.get(), "GET %s", key.c_str());

    if (reply == NULL) {
        std::cout << "[ GET  " << key << " ] failed" << std::endl;
        freeReplyObject(reply);
        _con_pool->ReturnRedisConnection(std::move(context));
        return false;
    }

     if (reply->type != REDIS_REPLY_STRING) {
         std::cout << "[ GET  " << key << " ] failed" << std::endl;
         freeReplyObject(reply);
        _con_pool->ReturnRedisConnection(std::move(context));
         return false;
    }

     value = reply->str;
     freeReplyObject(reply);
    
     std::cout << "Succeed to execute command [ GET " << key << "  ]" << std::endl;
     _con_pool->ReturnRedisConnection(std::move(context));
     return true;
} ^wrxwPyKh

---------------这里大部分是一些Redis指令
使用一条指令时，获取一个Redis连接对象，然后执行完指令
后立即把Redis连接对象退回到RedisConPool里 ^cCcckEvr

处理请求需要的业务，以及填写响应的body ^MFjxcMLy

Grpc ^3O5UShME

StatusGrpcClient ^514ISYjC

VarifyGrpcClient ^XwDjfHKp

StatusConPool::StatusConPool(std::size_t poolSize,std::string host,std::string port)
    :poolSize_(poolSize),host_(host),port_(port),b_stop_(false){
    for(std::size_t i = 0; i < poolSize; i++){
        std::shared_ptr<Channel> channel = grpc::CreateChannel(host + ":" + port,grpc::InsecureChannelCredentials());
        connections_.push(StatusService::NewStub(channel));
    }
} ^FgKbqatW

VarifyConPool::VarifyConPool(size_t poolSize, std::string host, std::string port):
    b_stop_(false),poolSize_(poolSize),host_(host),port_(port){
        for(std::size_t i = 0; i < poolSize_; i++){
            std::shared_ptr<Channel> channel = grpc::CreateChannel(host + ":" + port,grpc::InsecureChannelCredentials());
            connections_.push(VarifyService::NewStub(channel)); //这里用了移动构造
        }
}
 ^JhLaSx65

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

 ^eW43Cj00

GetVarifyRsp VarifyGrpcClient::GetVarifyCode(std::string email){
    ClientContext context;
    GetVarifyReq request;
    GetVarifyRsp reply;
    request.set_email(email);

    auto stub = pool_->getConnection();
    Status status = stub->GetVarifyCode(&context,request,&reply);
    if(status.ok()){
        pool_->returnConnection(std::move(stub));
        return reply;
    }else{
        pool_->returnConnection(std::move(stub));
        reply.set_error(ErrorCodes::RPCVarifyFailed);
        return reply;
    }
}
 ^Ihi9DTbM

向StatusServer申请供客户端连接的ChatServer ^kouhhCsp

让VarifyServer发送验证码到指定邮箱 ^QQQ9p615

user_login:
    [error]
    [user]
    [uid]
    [token]
    [host]
    [port] ^WaGTVMPA

reset_pwd:
    [error]
    [email]
    [user]
    [passwd]
    [varifycode] ^RL0CQidm

get_varifyCody:
    [error]
    [email] ^dNfC1kGp

user_register:
    [error]
    [email]
    [user]
    [password]
    [confirm]
    [varifycode] ^lNph1gci

http_reply ^pWrtuKuE

StatusServer ^uSxyqj9k

GateServer ^qru8t5TC

#include "StatusServiceImpl.h"
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/random_generator.hpp>
#include <boost/uuid/uuid_io.hpp>
#include "ConfigMgr.h"
#include "const.h"
#include "RedisMgr.h"

ChatServer::ChatServer()
    :_host(""),_port(""),_name(""),_con_count(0){

};

ChatServer::ChatServer(const std::string& host,const std::string& port,const std::string& name)
    :_host(host),_port(port),_name(name),_con_count(0){

}

ChatServer::~ChatServer(){

}

ChatServer::ChatServer(const ChatServer& cs){
    _host = cs._host;
    _port = cs._port;
    _name = cs._name;
    //con_count 的变化应该由实际业务事件驱动，而不是由 C++ 对象的构造/拷贝驱动： 这里只需要简单的拷贝赋值和拷贝构造即可
    // cs._con_count +=1;
    // _con_count = cs._con_count;
    _con_count = cs._con_count;
}

ChatServer& ChatServer::operator =(const ChatServer& cs){
    if(this == &cs){
        return *this;
    }
    _host = cs._host;
    _port = cs._port;
    _name = cs._name;
    _con_count = cs._con_count;
    return *this;
}

std::string generate_unique_string(){
    //创建uuid对象
    boost::uuids::uuid uuid= boost::uuids::random_generator()();
    std::string uuid_string = boost::uuids::to_string(uuid);
    return uuid_string;
}

StatusServiceImpl::StatusServiceImpl(){
    auto& cfgMgr = ConfigMgr::GetInstance();
    //-----------
    std::string chatServers = cfgMgr["chatservers"]["Name"];
    std::vector<std::string> words;
    std::string word;
    std::stringstream ss(chatServers);
    while(std::getline(ss,word,',')){
        words.emplace_back(word);
    }
    //-----------
    for(auto& word_2: words){
        if (cfgMgr[word_2]["Name"].empty()) {
                        continue;
                }
        std::string name = cfgMgr[word_2]["Name"];
        std::string host = cfgMgr[word_2]["Host"];
        std::string port = cfgMgr[word_2]["Port"];
        auto chatServer = ChatServer(host,port,name);
        _servers.insert(std::pair(name,chatServer));
    }
}

StatusServiceImpl::~StatusServiceImpl(){

}

Status StatusServiceImpl::GetChatServer(::grpc::ServerContext* context, const ::message::GetChatServerReq* request, 
    ::message::GetChatServerRsp* response){
    // std::string prefix("llfc status server has received :  ");
    const ChatServer& server = getChatServer();
    response->set_error(ErrorCodes::Success);
    response->set_host(server._host);
    response->set_port(server._port);
    response->set_token(generate_unique_string());
    insertToken(request->uid(),response->token());
    return Status::OK;
}

::grpc::Status StatusServiceImpl::Login(::grpc::ClientContext* context, const ::message::LoginReq& request,
         ::message::LoginRsp* response){
    int uid = request.uid();
    std::string token = request.token();

    std::string uid_str = std::to_string(uid);
    std::string token_key = USERTOKENPREFIX + uid_str;
    std::string redis_token = "";
    //从redis中查询token_key对应的token
    bool success = RedisMgr::GetInstance()->Get(token_key,redis_token);
    if(!success){
        response->set_error(ErrorCodes::UidInvalid);  //←应用层：但业务逻辑上uid 无效
        return Status::OK; //<传输层：请求送达了，服务器正常运行
    }
    if(redis_token != token){
        response->set_error(ErrorCodes::TokenInvalid);
        return Status::OK;
    }
    response->set_error(ErrorCodes::Success);
    response->set_uid(uid);
    response->set_token(token);
    return Status::OK;
}

/*atoi vs stoi 区别
对比        atoi (C 风格)        stoi (C++ 风格)
头文件        <cstdlib> 或 <stdlib.h>        <string>
参数类型        const char*        std::string
错误处理        无法检测错误，转换失败返回 0        抛出异常：无效输入抛 std::invalid_argument，越界抛 std::out_of_range
空字符串        返回 0（和"0"无法区分）        抛出异常
非数字字符        直接停止解析，返回已解析部分        抛出异常*/
ChatServer StatusServiceImpl::getChatServer(){
    std::lock_guard<std::mutex> guard_lock(_server_mtx);
    // 1. 默认选第一个
    auto minServer = _servers.begin()->second;
    std::string minServer_count_str = "";
    int minServer_count = 0;
    minServer_count_str = RedisMgr::GetInstance()->HGet(LOGIN_COUNT,minServer._name);
    if(minServer_count_str.empty()){
        minServer_count = INT_MAX;      //此时服务器没有启动，把它的连接数设为最大
    }else{
        minServer_count = std::stoi(minServer_count_str);
    }

    for(auto& server:_servers){
        std::string server_count_str = "";
        int server_count = 0;
        if(server.first == minServer._name)
            continue;
        server_count_str = RedisMgr::GetInstance()->HGet(LOGIN_COUNT,server.first);
        if(server_count_str.empty()){
            server_count = INT_MAX;      
        }else{
            server_count = std::stoi(server_count_str);
        }
        if(server_count < minServer_count){
            minServer = server.second;
            minServer_count  = server_count;
        }
    }
    return minServer;
}

//这里是往redis里写键值对  ： utoken_+uid , token
void StatusServiceImpl::insertToken(int uid,std::string token){
    //std::to_string(uid) 会返回一个临时的 std::string 对象
    std::string uid_string = std::to_string(uid);
    std::string token_key = USERTOKENPREFIX + uid_string;
    RedisMgr::GetInstance()->Set(token_key,token);
}
 ^6SHKlXox

## Element Links
yzAHMr00: [[boost::system::error_code ec, int signal_number]]

AHkMXJ9t: [[beast::error_code ec]]

ymklfVqf: [[beast::error_code ec,std::size_t bytes_transferred]]

qRuDbHzD: [[beast::error_code ec,std::size_t bytes_transferred]]

MOF8XX5B: [[beast::error_code ec,std::size_t bytes_transferred]]

P4NinCp1: [[std::shared_ptr<HttpConnection> connection]]

## Embedded Files
7bd3fdf21457ed97231a8add865d24db24b2d763: [[Pasted Image 20260912101327_950.png]]

80c53529bc7bbb0d2699c7388b2b55c558c36965: [[Pasted Image 20260913192715_147.png]]

%%
## Drawing
```compressed-json
N4KAkARALgngDgUwgLgAQQQDwMYEMA2AlgCYBOuA7hADTgQBuCpAzoQPYB2KqATLZMzYBXUtiRoIACyhQ4zZAHoFAc0JRJQgEYA6bGwC2CgF7N6hbEcK4OCtptbErHALRY8RMpWdx8Q1TdIEfARcZgRmBShcZQUebR4Adm0AZho6IIR9BA4oZm4AbXAwUDBSiBJuCAArAGsANjgeAHUAOVw00shYREqoLCgOssxuAEY65IBWbQAOOumE2YBOZJ5p

xYT+MphuZwm6xZTpiYAGCYS6gBYRo/nNyAoSdW4eEZHtC+SRxeOfnguri7HOp3KQIQjKaSjBJTBYJHiLC5w/afa4g6zKYLcY4g5hQUhsGoIADCbHwbFIlQAxCMEDSaYNIJpcNgasp8UIOMQSWSKRI8dZmHBcIEcgyIAAzQj4fAAZVgmIkgg8Ytx+MJTUekmeOLxBIQcpgCvQSoqIPZEI44TyaBGILYQuwam2Np+ILZwjgAEliNbUPkALog8XkLLe

7gcITSkGETlYSq4ADSYvZnMtzF9EajRQECAQxG4CRGhemk32IMYLHYXDQyQufGzDCYrE4LU4YlGde+dQmIwm0eYABEMn182hxQQwiDNMJOQBRYJZHK+gMgoRwYi4EdQ6YjIHnCbJRaohtEDg1cOR/AgsksvPccf4ScNvqYAaKmA5XCYVAAXlQAB0IDgfEoDYZJAIAbn/DhoIdGpogQVAsnTBCoJgjgwlIMwxFQAA1YVCHFGAZSYbDEOAaDUFQUg4

GwVAAHEECgfDSEImASWIRCAApGOYgiiIAJQQABHABKaimJEDDUB4piWLYgTBXE4AAF9oLU9DkOYBCGLk/iYCE4TUAojgqNVGNlFQTJcClX9UBGNCNOgrSdN4+TBMFYzKNQGMoBWKzSHxUg7Ic7zzI4SzrNsv8eDQsy8Qs1A9E4uzkkc6Dk0oAAVfpKmYD8om/P9AOAthQPAiA0Ng5l4OURCXLqqqMNI8xEPc4iWpwkyqJoui3P0jjuP61jBJE8TA

igKTmBk4aFKU4yNKcjgGsQ2bRqM7rUHCyL9Bs/AQvSzSrVcvSRoMzzNt8/ymCCg6woSiKrN26LeDiraHss5LEL/NL1IyoNOCgGVCCMcRUCSHdEjmbsLgPY5Ej7BtxUBgAxXB9ClZ1UGBZ9+gAQSIZRq3QYJxQGcsmCgcwCAJ8Fiege0xT0T8YyYMM0EzK8G3JcEYwIHLXzygqvzskqQLAyDoOqlkdJWprMLIvD9JIrDWq80zqNo3S+LOwaZtOhSx

okybSGk2SdbmuBlMWqXluOurtfawz1fi1jHqi/a/1CjglpWx39MUuAXZ8nJrsC8k7o17anr2uzYvut3PrYFKfsOsVcCEUChNYUHuDxIQEGvVmAAkwQhN97PiRHOikUIBagAAZGNzzHCdC5PXBNCCAAFNhWCpzgL2lIoVM2EoygqCQACEAEUmjqfBknoZRMATbAEmL4TCFISQRguJoAFkxW6MGIECbAogizEQWGG06lWbQvgSWs6lOY5Fmmesa6x3

ZoW0U5X4XGWD8ZYJYQQPGIE8NAPAeDHG0BMaY8xEiIj2CsY4yQQSSDLpCG0n9tCLAmBMC4n8eB1DmHvPYaIr5g2xA2VUepuTkipHSWkSApw1XdByLkpImF8nIBhIUIpyZIylLKeUp8TT5h1GqBAGpIFahtNIvUBojQQEkcmYQForSjDtA6J0oxXQNk4V6H0BRAxIxDAgdmqBObRljLfdAuBcIaK4WmX048ygn24MkbMo86G5lHPZHgZxCGzEIRTS

sg8bRHgic2DgrYODtmicE+Y99oT9iHMELcrdHztxrtOLh85MjZFyGY1c65Nx3htAsXcdR9yHmPDXU8LcbGXmvGwW8gSHxPhrpIOu/Qm5nnvG3EeY8GyT3QFPGo9AmikBnjPegx94CnxfMImuDixgfCrkCSYyQfjHBISCH+Exlj/2OHWdBew6i7hGBghsECoHYzqNoe+H8JgwIRIsF4iDMHYIriMOI1cyjoiNLQmu9DCSMN5OgakrD6TsJZJwzkUL

ej8MFMKEpYpJTShURI0kpo6G6nVJqHRhKZG4ryviqRDZzSSDcaSmu9pmT6JdGCsoxjvTLnMTXYM6MrGBNseM+x8YmguNTNojmbT/FVPssAr5cxFixKrKMcYSqWxtjBssAh5wEYZOHDK7peSygFLnAuEpXLykbmyfZGpe5uwNOmEXIZkqsxNI6YSLpbcQSrMqLtFCdVtClVAplCg9dfWhG0gGoNbAsWA2BrnF0AMchowxvgLGOMa6rNpkTSopM1ll

ArFTdw2b6agTgEzQGNlLSkGsYKxlrF/D81yhIP1kaECBvFunTObBs4gzBvnI1kBTwIFLuCHBldgmYP6a+QZLTDXXk7j3PuahlUuvwKMooHjyiBIgGmgAqkIfQMAqgNwHHjfQFwADyRh6AzygN3CgMpRXeuWZUc+l8MRsIbBs++bxP6zHhOcQE6CLhHJ2EA6YLzPjjFOLDZ+sDQP3JJTaeEj9dyLHWMkI4KSPi/LHf89BUHrkfyAYQ2G/wqGfqxEo

yFPDoUQEpDwcUiwED/DFEyRFKZuE8lRQKQRmKgyiIpYqKlKoiWyOQ6gOBQKBDieE8aUTZpNF0olfZXRzLYAGLZZADlpi0ArgsXy2tUqa4xk4g4iAuAABaYriD0rQB4ror6ay+JxAE7gXzrgAsRIqhsFY4mjBiX5psVYElJPBnWYJBy6l6qyQar1DYTXECKYuUp+nszFAy5ACZEBnDd21U0KeWU8aLAEkIAc+Bi7JAAJq4GIJgVIdxPHOcceHKgGW

/GdEy117LO6ZTHDxt3XCxdcLMDxi0ZIwk930QPllSUmAiT5szS1yzbWIAdezNysoa4rUyqLDuO1B4jyOpPM3IeXM3WdOGbkqdzB66zuu2EDdpQt05b3YsKzbAUbMRsy+nofJco31GGsC4/9rjwwuHU7s1wNgNh/pDqYPZ9iQ/IcEwEsOa4PIUfZA87wTl1N3LAk4Cw8Pl2eDJyz1DqNkoYXRqkTGWNsYRayLjKK+F8YxaKQTOLxGUuVDRiT8isTw

IF/JtRimaXKfs2p7mejNOsrdOyExFrDOhgFSZiewqJDMls9LutZQwh7ducQ0h5yMcFpC1E3gQC1XxI1dwPYcIfjJEPLFpi8WbuJZnMls1S4CgZcc71yoeWCtFZK2VirVXav1ca1l6AK3hT4na11lSm3LWVMCft2p9TjtOpafrod7qPc9M8c2mFqBAD4roABCNAARtoAewNADePoAY7lACAHrBcWdFnDOGwHAOAAB9YQUAfzaCQvbdt0aQ1hokJSS

vtfG+t/b2VDpqAu89/74P4fo//Xj87UmoGfaDF75TZjbgGbS+vhLbmhAZMxSFupvgS/fJGYgmZlEVmNb1eurKDzRt+Ap/l+r3r2bzbw4GjU72717wH0zk3xWg7SXy7SznCAPzQAHSdRHT+VGCrlu3uzOxyRLyHUXXwF7n7lXVaWHlKE626wnh3RDzOEK2K1K3K0qxqzqwayWX+1ayTzFA2XODeABSLAQQwwQULAp3h2SGeQSASFg0+EQT3hiyQyF

2gQRG0ASH+HhDOC+X+DI1J3HTeB7HOH2EkOOGfk/iR0o1BQFzZxhQZ1YwuHYw4VZzp3ZwEU5yWwlCE15xE35xp2JUUKkxFx8P1E8IU28JrlpWl1tFlw0yxl3G0wgF0xVx5UsWMy/2yy10cWpTCO92l0c3jw4J4Fc2lUz1eHmGEK+Ft24DWDiP81C3txdB7FmHmGOEiNM0HH1U9U93yW9xS3NTKQbB2wz23GzxhhSTz3O3aSuzwMHSAjYF8mXAy0K

B6201KGOAyy21KEWK63hFB1UM7A0L+FhiISazAD0KLDIXWCkJMOCTGDWLTw7lxBJH0AxhkDzF7l8nGMJWFCgCnjMwsm4FyIyFS2sV3RgAPSPRPTPQvWvVvXvUfWfQywgH0GTlPnJE0DUAZEgHm1eNmNFHSx6zeH2UJKJP2V8yWPgR+BGFuM6EoMgGyGIB+M5D+IcwRMBJKWBJlAfQTDYAAClPQtQmtETkTKhUT0SBSsTiA3jcS/QstliwBpMKSqS

KCdQvi8Y2ssE6sPia46TVSk91Sd1E82AqAQQghpwKBi9pi+k7sBlcDUBDVnsqCg8JAOSKAuTeT+TnwVsfUgdoEgQ3g5g9hzlpg6wyMTtv5wNCF4gsMSxP4vlpgzcz97hJNkgFh3g1h/TTgjhlh0kGwsF8NydzCaFLCnDrDmNbD7DONvcrDoA0V+MucREedDQ8VQiDdxM5FHlpNRdgjxdmzIBwjVMWjv85cYjDEa4Ei+ikijNP8LtNdzN4wv0sjXF

VMC81F3MbQTljD0FDCKifSv4LdIk7dEkwYgz/hzhJg3drV50vdClfc0tpSutA9t1g98s6Cw9GDI8WCY8MSnMODVsuCNtOh1iIABjrUs9DsHUxi10JiPVHtpifUJAgDW9AAIf8DR0EkFQAUFQBQt0GwEAEh/wALH/ABP7UABS9TC6NVAQAWjlAAQty3zbUACAGavQAMLlAB36MACEbQAGJVABZlSIoIsAEMYxCwAIM1oI1oDIRJABABmEsDhwsABh

/wAUGVABqFUAHVtQAVutqK6oXjSAFAwh1LUBABTuUACnlQAeL1AB0JUADRlBQQAWeVjKTLABO00AFWbWiyfMvCABClvZCuAVC9C0inQbAXCwikiuA0CCi6ilaeiqvZi9iri3igS/2M6QycSg2DyOAaS+S5S1SpiPoDSrSjK3Swy0yiyqyuyhyvfeNMGXcI/dGE/NABM6AfGQmemPNW/Sme/R/dAMtCtFmatFI6cyAH/PmP/JylytyjyjCrCny/C4

i0i8WIKmihCUK8Kzi7ivi/imKw2YSeKi2RK5KxSlS1ANSjKzS9KpgHKqy/K0ywqhAntJAhNW0hLJpEuDAm0LAnM6dRuG0y8ppQg4gldK3Tme017HdKeZgKePdZgCYarKodglZQHb9YHV+f+RYbsE5REWBRBO5MMtAXYE5F5TzbY/Ya4OscBJM5Mw4X095WsZMinXMsnaBN4UsFYW5FYADd5CnEFQswIqsykcUA5bAH4cslnSs4s6sjnIRLFDwxsv

nAlcFVsyTDswIsXdRJTPwFTdMBlQc6IrTRXD0TlccsoXlNXTUmcuMbXQgXXJcjXHMGVKQ5YYhD4NGvcgLGsZo23MLMGQ8RIL5XVcZNouLDo/AiAJLHov3fTQC4CvbW1KHI7RpMoZpA2wvSYm6zo8/CuBjWfXagSbuIkVAQAWHNABCpQX1AI7xX2cDZFoigKHxHy7x8D8BjB/DAOLuyGLt6h/AAAMKBJBzA0KS7sA+818+8q7/Bm6Zqo1d8aVsonK

Z9q806M7s686QCwCi6u6y7N9K7fB/Ba6O966XAu6W626O7dreoe7ID+6YxB7YCJ9irkDUBIMiFglVgDxPhYYnakZUYKq01T9vVaq6Yr8b8Iki0aY6rehn8GxX8q02Ypy7QG0+r/8U7J7lB07M7c78756u9F6N8K7vBV6a6666ot6m7W727sBO6D7e7j6OBT6x8AqY00Ru1e1rrUDTtLRR1qaJ1KaXqHspiF0u4iDl0B5iZfqlTN1xkd0+9YE+96B

nBJAspuSKAeBNAhAYBmBuSEArMYArBIa30EAL5WbuCVU/h3g4QSwEaVgiE4QwMMaDwdisz75P5oNaxCa/CYFIMXdxgxgixxCDG7bIAqbx1ibodYE4QVg9kFUCzqcpaZEOaubpgebjg+akVuNeFWqazXDRaGzVEFbAi2zsdZawnlEuz0mFytEVbFEojHR5d7IRz2Uldtbg6gxkjwGhVZztcIbFbxUinjIESvEXNqS3M9s1gAVkyeAXdtzZVqqaj1V

Dznh0FEFEFTgBzstvb3dfbpiA6bz5j7y48csAANOARJZgGoBISQKocUDgeiQhKAarIQGoKoXCFGL8vI0+A05Paku4muUOzPcOnPKOodG05cm8aCjh56q0mdN6kZARl7IRyobZ3Z/Zw54505iYc5y56525v7B5tbb0+yWYOBd+aHB+85AhMxlfHsRHTc9YI8F4K4UksoLHFVZIFISHF3P4OEIBFYaq7xiuOBSHf4IBQsL4Qw2Baq7RtAOIiFYkQWz

m7m3m5nOJqs/kFwkW7nMRcWrwyWlsmRTJ4XCnUV+WiXAp5W30OZiAJlUp4cuIscmp1XflWO8odIyzc8FpuzVTXIzp3gQo8FVcoJF3BmghQ1sZ4mbsUM+22oiZqq0hBEMhI4c880qcbo1ZnWyAN5oYw7YMwN7551Mg7q3dIvJZkEOAHE28zYjYmU441YrrdYsAQt0oMYOll+Rl5Gll0hY45wTly4f4dYV4BG9YAVxUsAQC/AUIKAR454kcSUiuZc3

EL4hkxwCKf4lkm84EkR44MRiRqRmRuRhRpRlRtRgUpEziYU0gNEpbTEwgTAbE94vErrAk4k6985Et8k5ontmkjATkKdpkjN5U0gKAHUw0vU617UtUkIfU9FhsE0w06NwFnA9Nu0sFh0x8iQKzGeIkIG8UBAbudRgHQWDF2BF3FISQ65UhK4JYc3SAY5UhF5SHeEOpYw1t6qmltAOsJIdDd5aEeYKotlh63gFmqnYVosnjafSJ6J2JxwvjxJ4WgTe

spVtJ3VtVvUDV4VgInJwkHVnss+KXfs9TE1jWoxKpvTP0QCvWq1yChpo2xxfAU2tp8dj13cT4eGPZTxxsfc54Z+Z2uozFr4F+SQw1wgBZi82641WN4pIOvT9PECj5+1XPBh/Pc2rN+O96pOyoXO4A5CrurCtCkalL9y7C8a/ysiqiraTqBAOawAIu1AB2I0AHuvSipvQAMBcG9UA5QtBUA2LoJc6KvABeox4sAC/1WrlWMiKSwANz1AAF40AGz5B

iwAbCVAAvvTq8mk0FQEosAFPdQAIOU7LUAWgEBH1puuJRJHLBYJBEukLtAMvhrMLDusu/LJql9pqFZWpivyvKuaupuGu2KWvKL2uuu6uCv+vhuxvJv6uZv5ulvbKVu1vfvNvY0cgSrD9n7k1X700P6L8AGJAGrf7mqEfWqgGa4QH38uqIHeYOAm0dv0A9vXKDvepUvPKSfaJRrfKJrKHLuCubuKvqvuvpvGvnvXvuuPvBuRuJuHu/vFvlvVv1utB

QfqHECc4wY4u030C8zHrJ0IPrSoO/OCCuGvreHzs/qIWJAYArNNA4AWhi4BI2BxRpxiABJMB8AeBlA6ghAEwj5UXKhHB0ZOBMihhdGDhQEVhVggzQlEN0aiWYF/5AQEQiF4Y6krh7HHlglyS9gEEfg6loQPhfeyh2WoQzlkzLhVgcNCwQmeP2bxWWF4VEsHCBaROhb5XxOeUxapOVPRW5P/CtW5M8npPey1O2nDXjWWVymzWdPEjda6nrWzMTPLN

9BzP3EOmWsCjumiiCxCFkzoR5Ca4/WoRfXLcDzwt8aAmMNBmo2c2rzTVAuC2Q6KlQuDsI7wLIvrW/nwPek2GQXckNfTM+sBshsRsxsJspsZs5sT3Ft0PODDSdG74kwd4FM3hB7JBmZCQ1scjGDwIE+WGEjLMFVQKFHkAKOIFR1mY7hCchYXcl4w467gpgZCe+JcHEIEcY+OfKTLxwSYMYC+85Y1MXy4SyskmCrCTsp1VayZ1WMtBTjJyU5N8VOfZ

Nvhp076xFNaa4apsF0tbY9jOFmawKP1nZdZ7mzwN1gbis7GM4ysMeEMM0PAU4/WLtFVBhnhjnBJCO/GCjG2vIH9e+CbY/mHVP77hgyVLNNlF1SIxd/mCdP2nmzmL+4uslbOUiWzWJNYvBzgV4PAiwyrBrOpwZ+IQmI6lBnApyd5MYSwxgCjw78HxGWz8FZZXgfBT4EYQWDnIDwlCLLAENOAvIyEBHYgZcBj49s+2A7IdmoBHb5trWE7T9q+xnbMk

5BrJHIMCX3SHpj0p6c9FehvR3oH0T6DEoKT3YSARSR7dwqewlJ1CL2nQOBLcjODGNAQd9dBD8EiFylH4kweDCHxWEu4P4j7Y0i+1+LND32nxT9t+zbqAc/2nIC4b+21zActS+AU0tf2T639Fe9/GDv9UqAG8EgcAbuDKGmAj97eGHNwjwQQQvIMBLLA5JMABSEtnA1jFIBQmaIApX4uxCPtjnODwJ4QCNT+NcmLDZlekuAwFGQJFbiYOa1AoTiX0

oFyt0UTAyvqkybKsC1E0tPwtky4FBFlWIRJkXwINYCCymQg7TlrV04GYJy+tIzqZlta4AqGkuRchZ2i6G5M8LuYhLWHmB2CHODtTFmqO0GucxgR4U8qeSMEAsuipg1LOYKAqWD3m1g8Ll813Q/NouV/Xfpmicq9RkAgAHfjAA0F651AAwDGAAKV227J0XRHo70X6IvrXUyqUPKAMfjfpVU4eX7VHrumvxuE78xaeMW1RfyVose9TetLj3x4BjaIb

oz0TnV9EXVaG/aUgAXDQJMNdCT1G/kC1ervC/a/bFXjw1IL8MwAlBL4RIHFBZQEwmjIkA3GSAyhJAggaYDKH0BWZZwRgLKFZkWC/8z4mjD9NfBho+lPg9LeYFhjmALAew2AiAFAMY6fxn4IGPYCjXRGjBBmMwIMtcluRkJn4CNSmhxxeAzB74vwE4EQhfiQ4SRFA+jLChYSUj6BgtGkbWTcLYpJOjIl3mwNk4cCG+5KHgdyNb68iSmggipjph77x

sJQ/fcUYbSkHloHWORcfhwWSEwcLamef0pcHxor9HOd8VNuqODbhZ8OhYQ8K7i9qZJFmxgvfj7jMEeDOgD5HLMJEWAcAOAmAQgDAEvTKARg1WKAH3gHDOBCA9Ec5t3Bnh3MXWv5f/v+VKBH9dslo4YpHWokx1MJcdZwZL1rh1j2GLghAA/2oKVABIRgeiIsGUB2Zpge6BuPen0BCAmg8IqeFZgSBAiPSP5d9Noyw61gDgx425AYT2QIh7OxyFYCk

EuAED5g0ISYDuLo6oAMM8Ca2mQgQSAgSwMCHQhXEfFo5eWe8SHHGTiGfi8+pfH8awj/HIoAJjAivrrSr5gSxM7A1kZwIgncDOR3ZOCUrQiJ8jTWwg5XGhIM4SCJRjTRxMJBkEtDlsBExQcRIdw7hlgRYdQcFkok2pqJ2okNilOZa1hLihosySYP36mi0JibapFaJ0kQVThl2QyUr2MmQc50oLdsWMkf6VBuSmgLKBwBGBWYBwSYYEa1WhrrIVU3Y

eIOAOhy7ENxcI+mvECSGAhky4hHcOIVPE1gTgMwBIB/H/QAp7xMvVACcDKmKcxWFUikdK2E7Ui6pdZekaBIlrgTmRLU9sm1Opm5NOp+TMoDyNVo9UhyWnUcqhItaijDOl0rCfGApC4SzajghUZUVfgUlJCxHGiVbheBSyNp4WJGqUVOBqjvOLE3zonUZABcjpswh0pZIkB8SBJQkkSWJIklSSZJck6rApKUkJ41sak3tiFysHaTz+d1dNr82zZsS

nRBPCAC10AAA6SxTwrQRAAYhaAA280AClxoADZTViqgEABJhKgEsqmUY5qAQAAgMgATfjAAM4k+zAAgZGAADeUoqAAsOVnqABEBkTlWVE5gAReVAAgAlcVoIpXMrn7IDkcBs5vo96FoD7zOAAAfMJUGibdE5IcwAHR62cwAHgmgALO1AAAHKABwJUTmAAV+MAB7aoAGW/QADnm/ohLjnTrn+yg5YcyOSxUTnxyTKic1ORnJzn5yi5JchObHMrnVy

OAtc+udBCbk+iW5mgNuZ3ISrsRkSPc2Of3KHljzJ5sc2eYvLB774wxcRZGND1TSw9cY8PL+oj0TGNVP2KPKBWjxwnAMMxnVLMd/kgZ49+qXs32evI4AhyI5Uc2ObvP3npys5ucgua3mLmxzS558quURRrnlcb5jc5ubiFbkdyu5b88SB/ODkDyR5E86efPKXmi9Lq4vPOOWOmLDoqx/yGsa8JMl39Gxn1FsT9UvAWTHS6AA2YJOEmiTxJkk6SbJP

kmKTfpKko0suN4DKz4gJYIBAR3hgmM4Ryox+NCH2zXIEciIRGagBWBJBPeAZEHA0nY5YzewcQOsGQhdw9grgyIwVtx3IHlTKBlUwvvkjoE1TS+gE5JoqxYFUza+UEzsozOb6qcep6nRCfyOQnxEuZYgnmSNP5na43CXGPCXIJdaT8iJK5GVDuHvg9gwhwzE4PZ3lmlVewQCL5PDBVk+cXhmsk0b0W5nbYLRSbCOrYIuluzYuN0twUF0rZeDZSpbA

CqkJ6zOAPFKQGBN4oilHgEyVbd5O8FITjAYRYS14HUAqELoHiBgYdmeylLjsogjQ44coFkGPCD+wJbsb2OwD9jBxw4tgKOPHGTjpxs4ndkKTGEHtRSCJcUqOy4lzDjlRwfpr2BOTiFAQSfeFWcD3ipJjyOyX9AcJA5HDGSJwgEvOx3SvT3pn076cMN3YolIVEwmFTMLvJdZ5h6CSWaQlRUZTXg6KlYo/FZWSF2VsUy4KjImD4qpaKpADhqX0nPti

Atwq4fcK4LGknhYHR0bIrukwVVFcHdAHUE0Dih6AHAC4MGBgCehL0hAASOKD7xGAiQmgGoAfBiZGLh0AA3gMmUgy8tEQttKQsYThEnA4ECwe+LWGCSoyEQbi8Qj6scYvACOr8TIblO8SQYwkBhBYHGXc4RKqMufPGeSLhQ0DGQCS+JvRmSV0iGpDIymc1MgmtToJDM6vt1MKYITGU7MhXIKJEHCj9OGEvmWkTGmWYhAk09prUon6zTGlgSLFjH3R

zDNwcLnTaUWC8zAJcMzE9oh7P84jLFlGkwYqdKdkRcXZDgzNg6NnWQAFlBbBYsWxlK+C91GykNSoTDUojIc6CIsMcSwxPizgN4mZkmquX3FB2tymofcrHbyinl3xF5dcPpI/qpVDQr9hKrQW0kbhwG61nmzTTKArcRky0mqqmIaqcsB8egMwAPjzA90LQOcV6VMXXBYELyQZoiB3AhIziEMo5dclOAHFAQXwXsG4pHU5kiRdMoVlErTX58M11UnN

bxnL5kyC1FMlVukpZG0yy1HUitVTJZnFMa16tOtZzKFFmjhpIGm1m2twCLIhZcokWR61ZZAgwhWg1fpMzlmr8dBNoWGEeB2nZ9p1PtLdf7S1mjKmV4LHrJqogDckeAVmQgM4CMA1BMAM8C4BwEBrdwhAl6ASDUBGBZQkFdm5SY83Wwp4Xm4yzSZMs+a6S7RjgzdUaPi4SBiKgAWDlAAMCqUU/ZUlQAIyugAE2tAAkOaZbKKgAY8jAAzYqAA8pVsr

LzUtRFErTloK3FastFW6rQAoh6JoIxUY8BZ7LjEIKExP9FaX/Qfypj0eZQTHqguta9VMF0DdLVlsa1FaStrWmrcItLFiKKxDDaXswzeBy9ax8G/aR3GbEkFlF5BR6YI2ekSBHNzm1ze5s83ebmAvm/zYFuC1zjwtWHEBKmTqQoqTcxuCGVshODXjb6u4TsG4sQT/xCEgza4IitWFSyU+0CY5d8AITB9rGpCHcUxtJHhNWNv4omVSNzWkzgJjUotQ

Ljr5sj2pHIkTS4irWsyjWtarvgNNEEii++k5AfpKJMULlWmY/btfkV7WiybQvYYwgx1OAaDXgo68LDYzODQh1ge0oySs04ljKLBMW5dcm2CRqi9JLapwUMpmLuCdZyynwSkKPVdYwdpwMmlDtaV7J1hgS+EO/CRrHjcR9S9SVFoII3Knib66Yee3V2AamhryqaWUDaFQBgSyG1Dehsw1grRh6AcYcMIZXu6vBczHld6spJlte1dJL3fUK/WyrJV6

u/9rqTlV/82dvupVWaRVVeM3h90j4Wdts16z0AhAJoJekUZwhZwWG/6a7xprGFb1CqG+lhkgE7BawOxXsB7Wo7QgjiSA7HEWDiJw7OOuM9kemux1F8Ky/4pJfjpSa8auR/GmmVkzpnatYJom+CdTo76FLu+Mmoac2uXKD8pBwwFTRmHlFWdAQcwblnptWnBJOl+mnUWsCIQ3ICWZm1icluGWHTrNjOhXUurWkrqbRau2ZddI1k1VsFq8vCoAHO/Q

AP5GqAAANSoBAAWmGABfxUAAOpgwrK54VAA/vI+imegAF7dQ5lc1ioAGdlQAPfKtWwntAfgNIHUDmB7A3gYIM1diDpBlipQfa2X1wxPKF+mAvfoQK+tOaaBYNsX5NUUx/WtMcgo6pgMptGC3MSvJwN0HkD6BrA1fPK7MGiDJBiueQaoOrarqZYjbS7K23VjdtqqhXqXoUVHbvqfDFRZ8M17oBSA72YSDKGEhGAJpRi7DQDJXHSZJgaKksD2CKlwj

EgcCD+NcVRl1hPetHSTJ2Efi9LTlCwLsGYZwFYzkjlOFNcxqn1Y6qpOO+fSTLE7cbj2havjcWt8KCaslFOxWlTvE1q1NOUmypofvl3oTmdUq0/fGG2AX76h1+qxXvF2TDM/gWo5/WOoELNFSi0um6bLu1mlLotgB0Cmf1XXR0EtG692d/sgPJ1fuiFQAFTytchQNV3wqABAHUACjEQxUACK/oAB2/QAFgJgAQ/lAAFwmAAwJSkqABK6NrmAAr5R9

mAB5hV9GAAac0AAM6pgdHmAB2CzLmAB3WN9FSVAAB6Y+zoIgAZ0VAAFzaABUvUABCOuccABjfoACCgoqqPVDROUNj2x8rrsaq4HHjj5x64/caeOvGPj3xv4xgcBMgmwTkJuE0idRMYmuDQC8qvwZjGCGWqA2pMeIf/qSGxtkACbbIalXTaFDTpablsZ2N7G8KRx045cduMPHnj5XN458Z9G/H/jQJ0Ez6IhM+zGTyJs4+icxM1wM4YvS+kZMkW4C

ZFxeuRQ2IkWKLjtth07R2IcMQBcIJ7buCMAoDcllNvkqGph1MXYdQcgzIQjuDCkUY4cOwQZlMApoDHVCz8IVW4rCUzB34ywbsARoprRroEXHDIxjtpwEy2NuRxJfka40E7ijK+0o4LnKNy0t9lO/Vrvtp0CjpNDa2Tcfui5tHtcRgTtZZ0toUd9kiAsQw/oJFBtxm4WQJuS2aIDK1ZmuyY3/sXUn9gD8W12faJWMHbetlQHrq1C2MtcFAgACqzg5

+FDLQCblPnGyTgAAH0y5gAUAC3j8BwAMHafXQALOJgATMVVDiFMgwXMADcCdBEACJ8dJUAD47oAGNrSii10ACMOoAFoTcboABjtMKsaeZlj0vZm5sQNudXl7mDzeFI8yebOPnmrzN5uA/eefOvn3zOdD87+akqAXgLq88C1BZgusnSqwCvg5VWxixjuTSPIbfAuEOIL2qb+SbaKfkNYL1jBXZC2V1QuHnjzxJrCw8YvPXmfZd5x8y+cwNvnPzpF8

i6BYgvQWGKsFyAKaZEXmmbplp/xdadukWGLNTYpdI6fV72GLt6AGAEYDxjFwD4pAKVn6d6BN7IADiZwNy1PVGFrd5yYqXCLWB0sSwSIQBIggJrD6VUkGUY68G+SkIlpWZ7GFMCSHQh4MMOb4DmYsLRLvxhM2ffzTyN46CjZZ5fV1NX0lrqzeMtJXWd6kFL+p9awaU0bk0s621mgY4Hum7NX69s4hcYCEaF0rSNRqO0XWDBeCrCrxnqz/erL9qzmF

1DsrSZprInJWLpEAfIPkGnB9woAyAZAPlFxCZB1rN0ckD3WRJWRsA1AEOFAC2h0wCAfeCMPoC7ikB/Q/oMUElrXMpaq9OQJCFWk26bRK4gAb+jAAa8qABZRMAB2/qgDzaftvIVEPEDAGABUQqIYN3gNoEAB90QDfIqABa0yuOAB35Vht0tAA9CqAAKpVzqAALNVhvQ33oxAda7tEJB95mAfSQIMQAAA8RIHrkwHblcR2AR15QJUj7rkgoAokDuXK

C+KbcIIb0Ym1RFZvaByxHAAW+pGoM2tTrz0CW6JE+tvBfrgN4G1zdhsQ2ob0N2G3EERso30bmN7QLjYJtE3obuIUm8gHJsIBKb1NvMPTcZukBmbrN6gOzb6Cc3P2PN9uXzc/YC2hbwt0W+Lcls+xaLp+cklmUWHfJUZPV3g6AsYvVUs08Y1i4OeG3cmpDGPFBSKfV1in+LlQXyG9ZjAfWwbSt/60DZBtQB1bpASG8Te1sI2kbqNjGxrCojY28bOd

Qmw3eFtm2ybuACm1TYxR02GbpEJmyzY6TO2Obpdj217agA+2TbItjpGLY5CB2VIJYgw+tokX3UDLaRuDcZdWOmXuG5ltdIhp3QHwBw3cGAM4HwBQArMdQegAkHwCzg2ARgCSdMFIAoxnAc4/yVTgxYeX748QLlsEsLDoJG2kZjGokMD6Fh9gs/DDLMGDVbJrGH8c5NcDD4b3cBgQ8YLkMGYsdiECCSfWTun05GcrMrWqQVaX0VXidmSms9kt4E76

ajbMyTXTtqsM6m1LR9XR2fQDNXnEnRn3d+Vdo86PWBCA5I0QwzDqBzI5tfqVURXiEuVU5mdascmuH9prkyvcMggpygGVzcyiA5veBZ2mD7lQYSICJqDclpgB8YgDZLHEyh6ILQASJoGLhNw7CRix3kiU5COrnAqwUHJFnhCGN5gKCfyz2HeBB9orFyiHLRrIT4JR9KwDDPamIRqjx9yM/YNDCIQIIBmxCZNRlZY0VSBOTl+JXPuLP5XSzxD2s6Q9

LUVGmpVR+s9Q5p20OmzDRls0fqYcn7bWzV+EuzsdZtNnWPaqfu616aCFmi0OYZqjIGuTMb67yD+FI/M0yOrNiygPHHmUleG5BOWH0AOE9DFweA1WBIBFueZrL+iEypXa/DD7biZlqj8A37Q0f1jLD5kyy5XogDzPFnyz1Z54dcsQB3LMCCQuDkuR31YE6Cfy9FKRzI4gyHq5YG4p8f4t5UyVyGJjO23EiGw6Or8VSHSd2r8HxMnJ7SPqlFGirTMs

nSTo32N8KHla0pzLgk11G6HzZuq9McxJtnHBLD/2scCaDsOZRHOrozKlfiDMlhFEjUVIX6d3xNxR2G3GNZnPjO5HmzxXUAdmuqE9nF/KVY9aMlwV0A9ESpPbelsyu+gcr0MXRfZOx3mL8YsQDkCYCwLk78YjGMQGIBLi07Mhj/Do70cGOjHJj/QGY4sdWObHYoLO9AwVf6gB7gsyFzQ2XsoFxFlYq0xvZL0mWHTNhiy+Xtg45ZCAhAWcGuC7ieh6

AFAQINVm3gjABITQW9DwDfsLiAppizZZiO2LuMsMG5AVt48gzg4spRYAhNcjcWpSCEAVmPnMEPCjXCR/imEDGSODoJlgD9f55C8iV5naMBM9ttgDTdFmONzhJF4UfcLlnirlZjF0JvJ3FOZR1RvF7UaQkH7qn9Vsl5mwpfNXqsna1pzNPadKDLaAVgEMmV6fVEhj6/YoYiEyFedBlReyzfOt3XrM7Nb2IQK/APjVZ6IAkbAHjATB9IBJmgAcDAHo

j0AJg1sn8uFrtnznHZQIZYbUmqoqPEtq52Df64Q3nO1FEAOyzas2bclFgbhaZ/c/cuwwnGBA1YIywI5d6MaBhKuM/F6PxqkpMRr56jJuTHiwXuhYmulbZqpOYl2VzJ7leyecbR3hVkhxkzIflX8n873F+30bNFLzWJL5o2KOYf1PdwbVtTUbjWBwgXgwjyAEvxrCmbBzcSAzTag/iqFnxFOVWdI6es/6OJUxytjxJ3QHo33H7r9z+7/ccAAPQHkD

2B7RZ/lItGz15ls6FdkTwn4fMV+gCWtdwB2O18OKQH2spRNGd1h60h5ulSuIAgAXg3AAszv0BZixAVAP3awhMB1rE9gu23beC432AiFhAIAAMbQAKbmgAduCfro8wAFRy7Afa1q9fCw3tLAAMh8hsBWvqyOyHjGbDGqKvvcUkOtd4iegMIl8MQJt3YVMRhvBXQO8TbiA+j6vgABiVKKLX1/P0Gq+AAQ80AAECU1+LgyA4AJIASQuKrCAA9+NDmw2

O7G1m28QD7p4habx32QGd8tAXwqw7cmxGt1a92Q7vlt6273Ze8nf3vF3zgI7d6/bfXwokX23S3TkT3AAWPKAAI/UADX+jPOq9SVAAB2qAAuOUtAUBWvvNr9Zt0AD0ZoACB9QAKbWlFWG33mZBiA4AoEUgNoFCAfhu6dPhAAz64j4/Cfz8oGEXknuiRqAS17n8zGoBhB8A4oO659eFvE2RfnAIn/zdh/T327QQcUAr+9tK+27KkTX4vbNDwXk6WXk

gLl/tuFfifCt2G6V5xvleCuNX+r0162+Ax+gHX7tN14d9tfTrf4Qb+wAW+qwxAo3/AON/m9TfrAM3j2xN8vQVelv0Nlb+t829Q/Hfr4Pb4d8a+vfTvnAD77w2u+3eoA5tnuzTae+kAQfb39P+D44Dfe5fpkP8AD67tW28/tt1P2D8+8Q+h7fX/oJr+Jvw+05SPtHxj+x94/fvzMdXwL/J9U+af7Phn+SGZ/5REktPnyhz8nsV+5vfP28AL6F8V+x

fqv/0FxAi+4govQUWL4hE0bm+27MvqiIv89tm/fbp//LhL6H9R+qI2vxyMHc63R3IxMPAQ71pYswLkeEhjiwzBC3ja6dqa68WOYtnYSAGXob45eeXhWCm+ivtL6W+1vr75VedXg17Ne8fu77O+oEK77oB/Xp75DeEfgVz++gfv7rB+h5LN68+PvmRD3+cNqt4/WG3m76rISfkd6g+Jfk34cAWfm3Z3edfo94M+hfg36sBvDOX4D+nAP945+ndt3Y

PeRfmn7nebAZD6t+MPnD7aACPl+oo+6Ppj64+5/kV6iQI/tT5t2s/vT6M+U/qz76B8/lz7CBLgLz4yg/PptxC++QOv7i+kvv6DS+1/poGX+yvsTYOBd/u37Q2j/lLb6Goil65GGixowy+urDLaYtI9DB9TWGavPvZoe9mpda4Q1WAJAwAiwA3ACQM8KQDCQfeLyRDgLQAmAwA7QEYrv2VGJ/YUc8QMDrPwyNLhrVUP8JIQHAn8F5hTMqMv4xJmwT

pRxfAPYN8DIIO4uPr408QDDCoO1utirYO9Mr27cehZvC646AnkBJ5O2LiVZlG6+jO7CeerFVb4uy7vTqNqtTLU7tmSniMA7u+Etw77uc0jaCTAcZCWD6Cw6k/T6etEt0rYccIII48ud7rI5miJ0oF7o4zRO/D7OiHmo5HOKHmZLaO08ESBGAxwA5bVYB8E0BEgl6HsBeg2AAmDEA2AJDhzi9js7xOOMCHSzGaswFIQuMsTtRI/wwSliL44l4nw7o

qEAMlJjAzyHqIAO4ThYxrA8VjE44iMMAk7MsUDl265m0LvxySscLrx4EOC+kQ6pK4nnjLTuRTkToSeqwUu776Gwa2bbB5Lkp6DutLs06c600oNY8Oe2AQKZCyIMOrlEvVjcGBYLuLWDvIC/BPC3uFms8Fwquslw4uWAZs+47oUAMcAtA8wPoBVAAwJB7yO2zuji9gQzKF5gGmusc6mS0HCG6dirVHaEOhToY3pWhzeivi9gLqiK5rAAVqFLvIcIq

2wvIzRNCBDWQCOhjBqzyGW4I0LwAgjYcGMvFYAojGt27shMKLC7saDAnyHMCAoeyJCh5DpUaih+SmsESh9DpsHiC8mpu63IKnpmy869kFhjbi3mNpqrSHbOy7W4CIGMAnIZnsaFjOD7i8EBecxu6H30XwcsY/BsFE5TQB2rnr7YmXshuFuur/h1rlMqrtGJMWXJhq4lIm4Wxa/+9MPq6GumahADCmwAZMhAhIIaQBghEIVCENAnoLCHwhiITjy/4

0DLuFL2AQagCRBwQSYbSKfruEEBu0Qa2J2GAYa6YOeIIU57fuv7tYBuegHsB6geRim9pZuLuHAiZCcfLEIHYFHiviWKRQkCDfACZgQIU4yUgHwsccZLA6E4pGPFbBOxhKQjrktYO/BXAyTpx5ZGfboWADuFYYQ65O/IXMFTuonuyLLBzMlQ6ZGNDgS6VOKEo0ZyeDVq0ZKetjvKE1KSoQoJHBfagYjrAHwHGHDMlwKOHxmxugzTjGEBqaFNGrwQu

HwOvLMuFQUmujuprMnQLroHq+up4L5CdEQggMRqwIkIDGMmKUCsRSIBxFB83EU+ofUzuncpu6Dyp+qTs/6l2rvKQJDujhukbu5RWIsbvG6Juybqm7Uq4KuHp0qkeiezvqZoRsLx6xxAnoAUSeoSrTs3uolH56HyjuhPCUki0DigMoOKBWYNQNViCS3JAkBsAPAMJAtAnoCHoIkNKvuyHsRUVMKwqOsicR3s8eqKotk4qtnoZ6y5Fno/sOesYpigo

HIXoWavofIpnO8EVZa7oj8gOCtR7UZ1HdRmAL1H9Rg0cNHpuWjB/a4RJYCoRnA2GPsB30w5iRw7ABgo/BpkpymGwscmYSoQFuH8IyzNE3Lo27bazyB8DUcQZPDD6CzNCMGis5Iv25yh3IQi7TBKStWFiRBTmVaSRNYS3x5KbTHER76NVkS4MOWwQp51OTVr2D7BXOocENKvYf8C3I1nFLrahVuHsijhrwEqIZhjwSaF8uLkRXpHRiEe+6fuKEa57

uemEV54CyPnus7qSroW8HoIkwPDAORJ4El7qOfwf6EumR0foDJAqGtODcks4LgAowMoDUBQAfElUCbMfeKQBWYeehaESAxQUa4RhHll85XAYwN06zAAzKIQ7AdSAcBuxyZCjgYYsMPZzJSBCPAgICkhAeCqiQZNRK9BzyODEGhaMliwvASMWSL58qMUJG8hIkdjENhgoRJFk6UkYTELuJMdJ4ruxLv/ryevMtTFD8mgGMB0xWkV0yMxHrK0qxEUh

Cy6kE4SOzGiOZ4hSRHA4hPZzmeozpZ73uv+hM5PuszjuiLAFAFPCzgFwHjDEANQAJAg0noE0AUAdkllBZBhINLHyqqkr57yxArrMbh0dYMioI0qsVdI+hmsQ9LaxFzrgCeg0wFZjTAfeMoA1A0wMXB94LQPQBZQhAHugjAfkJehZQSIVYAOOVMg4iJS/8HZwwIQCNchz8cInvCBKXEXUiDM6DoPpBOFIaE7YiETrSH0aWMvSFxORHok4shJpiWGZ

WMLpyGZxJZoJ6zBucbWH5xowbO4ihKwU2HihZMVU7lxjDlTE7BNMbc4aRTrAcHaRTcTKhI4p5Jir9GxkV3GGe7zgEwfANGnzEzho8Y+7cSUzp6SuWFznjCaAdQJ6BZQxcJ6BPMFBI7rmigrvtjfAFHFqgnuXoQc4XxUEah6HRqieomaJ2iXbHyCIIo6qHgBwM4plCoUhI6RSOwP0ypkzNNFiSEx5CHGSYhYBYrGe/BHmGXqhYRC5EJbISQkchUTB

k60CWTsO6ic2ceTKFxdCXWFieOMY2H8C1VhzKsJFMe2GNWNcdcDdhPTO8zp8VtB/rXBMsjuBcxlxAeC6iN7tOZPBAscdLzhiRucivIjElLIIeK4Yc5rhXsvwEyBavFuHQMoyRn7KoyrpDyv+3Wh/5J03Jpq4ZUOruxbXhJALeFcWoDI+GWYd8Q/FPxL8W/EfxX8T/F/xACX+FQMTlFMml+QEZfSgRUvFIqYEkEftoWmgbjEEZsAIdK4zwKMAfBwA

EwMJCMQlvIQBCAMoAJDMQPoKQB7BRQRm6PR3hiviwwEhFcBgCsToeJkIXqimQu4QZEWA1IryPR5+EW/CoQAoBCKjrGMtScnwccvSlDI3IUhHWALSUslC7xJMKDx7JJfHqkll8lCaJHUJ6LrQmb6uSYwnExfUoUmKRq7spHrudiDTGgqPCS058JjcSG7HBqAGiqZSzSUI6DG+5IZ7A6JyPpGtJFnjLodJ1kV0mn8ifAjSwwZ8dHTqxvwVYn/BcQTl

gXAB8MXBNA+APBBCAuENMBXMZUC0BGAMAJ+z0QAAfbGOGsKSUFZuwZOUGAY1wAiC+qkbEA4r4CUpGQM078AxHgy4VlVQ7ahYOsDdgoCLwR+K22lMAo4z8GAL8qWIanGY6FUggCXqnwOQmIuMwVylzuecYU71htadJFEx1aswnCpxSkpEVxKkYp40xeMPXHNYe7gImZ4MOt8A5CRkTuJdKBYM0T5uZ5LInDxVkXJ42RR8fwRxkO4gMmORd7ntFaON

qTugwA+gDUAS+uEMJDigYYaCLhkxbhlLnArHGcFBq0absA7gMwPKjxSPwNilJmRyp8DfA5uniIw4rHv8hpGjKVx5ZWEwejFTBI7tWk5xjaTyn1pOSdym5KC7lJ4VOMniUqdp4qZIKVAtcVPAVJ0/GuRwyI6YYJdxuoaOH7AywCGZsxrRG0n8xs4Z0mGJR8Zg6P6C1uF4hAu/sgC7WMXl9CHWYvmIEbWfaH3inWmgDAB9AzADxlooKHIFB5gCXuuk

WaKXtBCQBqADclsBsARr7wB2gC8Yk+gAPixgAMSxv1k16sUUlIABsjoABwKrPJNeXFLRRg20gLIDrWLPjP6BAdWFxCU2/PtQB94cjOKAiZjmYEDCQBcLiBC+Dgf6AyQO/mtbMZ0Xgf7sZXAdxm8Z/GeEBCZAoCJk02x/tf7E2K1kxl0w5IFbYcgQgGEDEA2/hFmCZNIjFl5gsPgBAn+p/p4HtyRIFggsgQ4HVjDo1Adf4lZxcNYC3hhkOEAC+vtr

4E+w/0FibQM0mdl6yZLAWMlVgCmQL5KZKmRplaZjXjpkGZRmY14mZZmSd6WZ0/t3Q2ZmWfZkr+jmc5muZ1sSJCeZUAN5mb+fmYxkBZLGcFmaMHGbn5hZqAHxkCZUWRhB5ZxAHFnxZVEIlkBZyWYEB94aWRllZZV2blk3Q+WRBCFZD2WZCq+HcmVmaMNQJVmGurMDVnFZQOe3L1ZnIMEBNZuINQFtZGkM/6HhXWu/6cmn/gnbf+l4fyZ/+qdoAEmu

5Sj1R8WXWRwAyZcmbwyDZxXsTZvAI2ZpmoBE2YZkzyxmURSmZDduZlwA82cYFLZdmYICrZTmUIAuZTAG5lbZzWbtkS+vmdv4HZe/ntZsZJ2aFmgwPGRdnZZ12cwC3Z92Q9lPZ61i9mpZEYB9mXZkWd9miZd2X9nuBptjDkg5FWYBzVZ3gQ9l1ZDWQjkiQzWcjk6+HWSaYeuwEQ8m2iIQevZhBryXpbvJsEc6ZPSFzhQDFws4MQAHw5qtyQvx9EM0

DOANQJ6D6ATQMQAUAWEc5YOxgaU7FuW4GNFJhO1tNBhhmH4relnAoOLWCE4fTM+J2MyaeUyxqfwHDAHInnNbrxWAfBx6hMfEdx4ZxQ7pWHpJPGpkkZKUGfjH8pTaXBlCp9RiKlsJlMVXGcJZSUSB9p/qYRLypukdAgBxQUuSnaeOmmgA4y4iTqJBMRCKZ4WRE1vqk2a5ofZpTxM8XPELxS8cwArxa8coAbxfeFvHHETibnprOeiX54zGC5jB4m46

YWakGSliYHll618eh6X5s8fPGLxy8avHrxm8XeFhaDws7Gd61HpCLrkXKomHgi0CV2DB89bvimPIzyJYosxpNPzoJh2CcwyoyKQB3qhIXyLPzUS/6V3lZWPeZMF5WmMfmooug+QJqLBwoSUYlOYoXJHrBrYVKEcJMoTTEDgi+W/n26T7L2F9xzSucT9G9+gZ46iCCB6FEIU6mRm6pExqfkVxi6UanEIuGiSFrpasauG5s+bILEVs+6ksSHqnkT1g

EFleTZzXIxLBRrHEFBX4bRmghMEhRxEUdHRVCr6i8QxRH6iLJfqKepw4YApKr6h6xB8AbFGxJsWbEWxVsTbF2xkAGNEQqE0WKTFR/haVHykVwZewLRoGn+pEqdUSSqNRlQBHlR5MedkHx5iecnmp56eZnlyCyRQVGpF0KukXTRZ+WVEUkeulVE6RgGunryaa0ZcIZ6m0YqrPCG6ZfEgFYeeh5CAWKSMDpgX2PQBTwnoHUAwAjmsd4wACYFyH9pp8

I7F3hhHuchnIGfE7h7wqMt2BeqdEeITGagavfR4F2OHggbiBCPjhHgT8IWGQY3LNGQJqJyrXmxJKTgwXMITBcBksFoGVjEZJBMVkm8pWLjBliai7vwUth5MW2FlKHYUp4N6HDvVFL5KoYEjppwcacBP6q0sQiEZiQEFJeYx+csxaFUHjNYZ8GIXESGF58aMVWpWsRMX2a3JJswwA0wJoCXoPAJsxZQkgHfbVYOUPoAcAVmBcDFwryjCkPRQafCke

W8MCoQGCI6cHEwIO4sch7F8IP4wA6KImWB15KZNCJlu4MXqJRpkMWx5YixGOMDKiQUlp7pG3xTg7pxAkWjGspPIRQlgZwJaPmQZeMQXEglkJfBnyRiGR2nsJs+SIVlJKLNKmKhmxd4holAzgxyJq2JRqKdxdSd3F3w7jo0S3IRJQdLWef+pM7WheUA3ANwmgLhBTwdQDUBCA2AHUAHwbcoMzdR9EEYDWlDce/kuhB8T/llC6ZnvAAFGujSXAFT2N

uk6OEeFY5GAYhXc7hheeTvlTATRP3Gv6ZwT2BeqeyPgh9xVumQhoq0Rn4Qnk/8H5H7KW/N+nxWWGEkAd5qaj8XT4LKVmopJfeZyngZDCTQnD5LpY6WwZknhPmEuRSXCVM6whRu71OgzJhkdO6JZDDvOYCPhloA2GKOEoIMOAEyJl7EoHT8u/ntRlVEJiViXKONpItbLWsuYFn7+CuUdZK5VtuFlfZwmT9nEA4mUYVDJsYpUBUQXeHhVd4gAG6KgA

HnaIFiT6AAnBaAAdsbEUlFDpmAA1hpPmgQD4AwA2gCbwwA1AIADPsYAA28Wt4XyRFJRQkW3WUb5U5A2cgBw5jWS7lI5WuagCbZgoJwBhA2gH6x2Z7mdtkKVq/Jtz256FAoDpyiNjj6AAt+6AAfyk3eegYECyVzUNoCEgHPrT5EAjAFxCGomvrDZKAWQOoDJwqAKxTWUgAPD6sNoRBKV4ubiDaATlZIDJwPcj+B/gXOetYVgmgOtZqUUlcTbdwgQN

3DCgYQLxAJVIYFDlUQbwIjaMVaaCxXJwMAMr4rW+0MwD5lYgOmB2QDcGwCqA2ADKAKMfQPoDEBk3hOxkBHtmJXBAvEHZlqUb2aQBXg3AX3jBgBgDxnt0zAOpVX+0fjXZZVzFYABtToACABi3hsU01S3iAA88aAAD/HK+MlXmxmVJlZGCT2YVRtZRAk0PIDIABIBpXE2a1XJXtoWlFxA7VkoEEC5+rrtQCAQzriRAVggEMdXQ2dLFxU8VdCsr6zIa

gAgDZw61WEBpVxsFJCtZtsMTbeVm2R5nNZ/lUxCBVmWeJAhVqADtURV61nmy4gMVdDZKA6cmlo+igALGKcVahyJVCAMlXCg6MALaoAuPmjVQArFCT5pa8lIAD3sYAAh+roHX+GVQDbjVOVcQB5VRWY9lsApIFtDFVVoGVUVV5gNVVbWdVcgATepAaH4dyLVahyrWPlVDV+VTympQ2BPVX1X6AA1d5zDVyvjrbs1HPtlXzVc1TNXLV09qdUbV4QFt

WXVc2btWbg6WetZHVI1VRDm18lRdVXVhADdXrWCsKLkPVsrq64vVTte4raAH1bxXfVrEH0D/VZ1UDUTQINbDaLQ0trhX4VhFSRXkVVFXxV0VDFQbXMVrFRxXcVvFfxXfmglTl7CVnAOtZy1iOUNk0+JlQDXtoilZDUqVilaJAaVWNWnI6VBlUZUnV1dWdXmVuYP3gEAhADZV2VTUMTaOVsNS5VuVnlW3YQ1yldDUBVQVQjWhVNtSjXIA0Vc4HQ2B

NSlVJVTEClVk1r1elVjVWdZzXc11/gVUC1c/qVV/g5VZVVi1tVfVXS1CAOQFy1bVU/FMQnVd1UPevVfiCa16gNrVN1gdXrUc1RtfNWm1PNdJVd1FtUVXn21tRZm21+1Q7U1Ae9WA3hANddoBu1NtddX4At1fl6kA91RACPVCAM9UQAiDe9V51X1aA0/VEdcg1R1iDTHWmwoNehDg14oIrUqVc9fDW/gi9TA3L1VNRjVUQLdTjX418VUTUk1qVQVm

U1q1jTV01clEzUs1p/mzUc1rFflV81hVYLUX1qAFfWi1NVdtaS1Qfo1Uy1sOU7ny1SOfXXQ1KtUxBq1H9RrVa1Q1X/W61B9UxXaAQDSbUrVPNS7Xtom1VA07VE7HA2HVCDYHWuNqDWY3u1ntRtZ3VvtYq7+1RDYHUkNn1ZfLX+FDX9VUNzUNHWSQdDXHV+BEYgeE8GutAxbHhcdp/R/+idnuS6uApn6n3hQASTlGsZOU5SJ1SdcRWkVlFdRUZ18j

blW51MTXxUCVFOT1kl1HAGXUGNFdbTnQ2/jXXUz1flY3XN1Wla3UA2elYZVV1iTfJUWVfddZUP1Q9WDWY1CgKw2uVLFB5VeVTDcY1+VrDcFUcN3OcgDL1q9cr4b1QjdvWk1+gEDVyNh9Qo2gNp9UVXn100JfUi1VVZo0S1Utbo0P1zVQY3P1HVSIDv1vdp/X9VP9dY2INADYfWONi1c43X+/je43bVNtV4321PjYg3+NaDTA0YNWDRWC4N+DYQ3E

NwdaQ2xNp/vE2R1STTQ0pNHAPQ07NzDbPVj1bDYjXI1TAJFXIA3DWvW8NEzfw0XNLAMTVXNIjX9liNuIBI0M1zNcr53N9jQ80n1SjWfUlVrzWo3vNN9Vo3fN03r82y1BjcQST2ezVADaApjav7q1X9VY061oDVC32NMLSA2n+CLZbUeNyLXtWotjtatXgNrtYE3oNHtZg1e1oTXg1+12DQHXK+0TaHXkN4dQk2mVgNZS0mw1LWk1B2/gfcneum2k

8my8AeVvbDxO9qrwh566G2USAB8Jegow0wJsybMEwBhk9lp6RjRtsyYdnjiEJyCGbBGxCEUKYqTznhGfRpITEawwUMu2y+xnLvFbzAxYXEkAZzCEBk2lGMYCVsF47qi45KQ+c6V0JmSW6WXlCke2mipyGdKH3lTVjwD+lTTnrjtW7zJFgLA6DvIWjM57qVSvwZwe84Dx04XOkklCsQuF7AqpfRnQVkXrBXy5B1ormcZ4vCrlG5OWahWm5GFdSWSZ

TlNBAt1KmYAAU6k60IAlFIAClRoAAsmpM3beOQH3jBAEUOoCoAemfpmVywHVJSI2M8l6JcQBNsjZnm13qJBF1vWcX79ZpdcgBktczb81KZgAAVKgAFBygAP6pgAGV6UlAG2d15HboCO+MHXB3KA6gD5Uht7aKxWbcqDX2jGtxNjtVWZ3dHG6/V/OQ5muNKOdLZ/tEzYB3AdYHZB2I20HZJKcdCHUh0od5HWh0A2GHVh0t2OHXh0Ed3TetZkdvHQM

371NHQx1MdZDSx28dbHVq4cd2QFx2SAPHSg38dokIJ2gwwndDaidC2X3gSdfQFJ2C5wHbJ2zJL/tk0x2uTeq79ahTVvlwKV4YAylND4RU2Ouv7TYAKdJPkB3kdynVB3sd6nS52adBmdp28dunfp3YduHaHL4dnTUJV9Z0ySR3mdNdZZ2Vw1nYx3MdgzcB2OdfQM53wdbnf42ed3nb80aV/ncYFBdD9StmEgO2TJ06+dyXQyxtxhvG0sM2BEm1vJM

ESdrptNieh7MAGZVmU5leZQWVFlzjjVgnM5Za9qIFfZSvgU0vjoxLdgGngciAOfvB5azAF4kYSROMCAYQkhyUqRjY0ZLNtI3ADZWQW6EWYWHwaerHO8gyJXxbxEWl/ESMCCRvecJEHlDpTBnjtXBQ2lHlRcReUFJk+XO3T5JSapHLt9EOIV1KIZf2VRkqCP0Yqxe+cMaQ4xuDu2zpeqZRkGpIFbUhWKMcf0lLGEmasbORZoW5GWFHka5FZYP3URm

oy/3coXcqYAAEIg98Zo4xbiuYV4VO6L6i7p+FrRY8rxRBRW8oNRyUS9JMlLJWyUclXJWwA8lmQPyWClwpaNH5RlTU0VyCUelKTLK97LHonEuRdKrBFHumnrgaUqv0V3C7+cMXKqu0WMWtlW3fZo5lnoFkHFwyeZswjAQgJ6AowzAJOIzweMA3D0AdvFnkBpopbnkPO4GA/Diy3wP0zz8FwbelPOlBX5GJpnwIjF15myKeoJphCAjQ0ehrL0FJAxY

O2wHgOypW3Fp+Zt3lWllaawXIuI7RwVr6mrNwUVmvBUwnQlLCVPnFJ8JaUkWYmgDwDFwxPS1jL5UhVZxBWR4DfoKFpBPfBcxiCHsDIpppYPFf6Z7Uz1n5dnr0BQAc2KVhTwU8PQCXoVQMwAJACYKewtAxAIsCrx28VWV7x9sjWXQelwAQj9xq6Zz2YVQBat1Xx9JTlgUAmAO/Gzgu8MoCIAs4HjATAzAPoAn2VQIbzn6qffOLp9OxQWCxqZLD8AN

EbjFepl51wPghTpD3XvDjqs5cgJEI/8C4xwYSIIgiw6uAtfSqCB4PfC2CFHO31jBjBV30I9WcUj0D5IJaj2D96PTwV5JLaWP1tpsngu13lEqTXE8A3JAv0DpK+b2FcqD3eDEb9HMeOn7t3iMELkajbQf3jWxJcf3aFhqaz3W0VtI2USuN0pumnOXyRABPxHwEYCuA3xESC4Q1sc4A169EIQBTwbdPdGLi2A2gBSEhwF8AwIl6gAhRlWwN3pbIywB

D3EIIhKQhSyocXEBdBpYGmnTM5wM8XhxgZGX1YCYTgynEJvbduV/FA7SBlpJAg+wVCDnBSIPQZEGeeV8F5Th6Vlxk/beU+lS7fIM/SAZZr2ol3RVZzx8qMoeANJH5RPrU96/DuD/A6ZnhnqFQ8Yz3yJc4Sz1AgQqiRjwegA9+2rGtg+qoZt6ANyS4AxcPRDCQMADwC+DLQFPBCALQDKAWOzgMQDQa3ZRgPbFjqpcD4ILSky7RkxnlLI/wXwAcBjA

BGvDCE4RpV92SYsRORzHxFxMZ7B8befMJe89NHSl2cnoVD2d5MPZ31w9FZf7TZq+5faWCDZ5cIPycSwa6UyR7pQIWwlQhW0NyDM/TwANwSgwzEqDHrFISVtWIWqkaiKwFzHvpHhcHH/lxonMNUZh8UakRSCfFYMWpFpAH0HRoBfZpCAewAgCXoNQGwB1ARIBMBGA8yE0DYA1w4QDFwc2HOIOqGLI8M5hZNPEKxhUw1EMY0rLKQN7IcZB6qvAUdtS

yAjMQx/BkSfww0F0h+ETAjoYiCBhiHgcIBuWyRdCSjG8DzBfx5DtvfSBL99pVmj21DGPfUOj9jQ4SPXlxIxU2buPACn1rtvCfTH8J1I5bTI6ewv0baD6qa5zBIjLj2BMS0w4f2zDyZVNbf9ZJUAgUcQ+muqX8AoyYXa6Z+Xz3MqVhYL0bKvRvgg2jbsaHz2jWWMjKe8zo2sBao7owr27oPhcr21C7umr3PKGvZ701Rb7JONAay0X0Vgai4xBqkgM

ANBrEwyHrSWgD52hc6Pxl6HjARukgAOBCAVQGwANwRAJoCzg2ANyQowe6FTLKSGo6YqPDUOujjlu/Kpvm7iUZkWGBMN9Iiq36fwLRqAgpA18DSJnwIkNROHHNciPwqItiJNEATPZz0FiIzwPIj3ff6NjugY1UMD9OI0P2TuI/fknNh4/bj0tDpLou2kjaGTwAjRiYzKnJj0CKT2yooMXhEMjpBJ8UiOhnkeB7ANBeyNzqnI8z3cj5gzyx6ewQeup

c9w8Tz066FhU2MC9RbK2PATufRFIsxEE02zQT7sfcHGebES7jDj/bFFGu6qvXFFTjtUb+qu98470W/qpk1KqQa64zBo2DQo/YMyAF/QJBX9N/Xf0P9T/S/1v92ERd2Z9lHpBixWoVu7GwICGMEZwgRKc7ixkKYbkJuK1xC8h4CFY1AnGeJIePp1g2Q38BzA2IkAgfOrIeaVejlpahN8DdpUCWYjKPdUM4Tog8P3iDDZghnNDN5aROyDqGRICz9l6

JSMpjy/U0rnISoirrtKWY4oWbS2HKyzIgOqTMOaFJg6SWTKpGP/38jxhQ2DiTDY5JNzCzYzJNdYzjj47kaRCAiDxTnjscTJTsfKlMxknyOghaTo49FF6TgRer2GTM4/kXnTKJaEXFF08Bolh9EfVH0x9cffKOJ9yfXlFh6VvVCo29LRYyr29pHjs6FgpGETh2cd7ADN7ge8CSlYllyonrdF7vSuMXT5kz70gcBesAOaOdg1sNAQ4oJbJ1A3JMXBw

ANzIQBtyZCBQADgHALhAbw/g5m7ilAOhYr9Dgw9sLvDOwH/1QYGhLSkoIiQAC69gMAlcAIIqKfE6rldSE8Pkh7A7GQQxwKIUNblJZIzjqR/xX6PlDGI5UNYjJU/Xy4TaLuGMETraTj3SD3pbGMPlaHMiW7uVI61OZ4ypQMPMTVuFg6jD3SuSzNBo5Qz3DTvEwulmDQIK259xquqsPmpU03togD4xbuNgFkgESAIA1WDUAtAC+UW1OODLL9FkYMfL

ESZTT3YThxAysrDAlEoQwjJ15ZwHAivw9xQdiH5PyED0VwQCCoQZ8RwGkgmey0vCOblyE/TilkTOL6PspeagGOE6Yg3WkTtfKRCX4jM7Z6XztuswiXLtAkE+UHumeIfllCDHO0oFjbE0oXHYEjn04Ozlkee1ljkyoirIpHs8ubfBWFYIaVAbwOnKI+hWl75sAlAa1D++w3IABYmmgHyBZdhwBxAu4QTa42UlDj6AA89aNetcoAACRoACIKgTaUU1

3lJTDcTXhwGIU6crTWI+gANxpgAHoalFFJQE2aPiBYUGOlG/NNetcsNzfzgANvxgADABgAJLegAKH6gANByCC7dzXeOFNLbbzacrvP7zh8375KNp8+fPQ+l89fP22t8zjb3zT86/MfzLdl/OhyP80Nx/zRlQAtpyQC2AsQLUC6j4wLcC3gtlcSCxwtoLWC7gvPz+C6HKELEXX2HxAtKVmm3iufUeE9aSyTjmiGRTesnJd2yZmJyGoAdAzELpC/gE

jelC0NxnzDAU75Xz2gDfMt2d84/OyLZXO/Ofz387/ONe/84AtpaIC+AuQLLdtAuwL8Cy4sSLUlFIs4LYi+wsKL7rmabzdQQY8mhBK3ejPQRZlkG6xBQfbxI28vYFba68dlk0DJAl6AlXTAWUDPAP2VM3CnOxkhC9GBqGMvsiA9Ccx9pCEQfDuDriqwMGp0s/wJ/AUc8fB8VZDIfE7iAYXyA/QkhSEzlOlp5adCn1z6I4VNKzxU9hOqzZU3hMVTZT

qTFSDSGb3PT9FEzKDNTcqSbOjAhCHUjOMWodGXzSo4QGQUaLwINNFjjsyWMKJQsRc4IAU8KAQcARINySkA3cAmD0QnoN5IIsCAAODEAQ4u/2bR1ZcBX8Trsw0SHLk0xvM+zKS9YkijOWE8svLbyx8tfLPy/oB/LAK0CueTCqsGmJWpo8Z6P6kdiSE/wxnvAgeJBGthxWzmOICNw0LjjkLGatyK7OCzsaicAoI8Mi0oejPbvjIxKZaZ8AVp+U1Wmz

LffVhPBjNQyPkdzzaZVNNDkoTU51To0vIMXJXQyEUk9vQ507SJuYZoPEwKIlzFr9fwKMYjONy/PMjTF7eHRHASOECDQrTkaYW89c0ysQLT5hfiR0rcMUZosxQTKmylAcMjMBsrLHIDqEIh0zpMq9jKvOOu9RRdr36y2SxMC5LcAPkuFLxS6UvlLoerSrW9lfFNF/TaQnNEKksMw0rJ6CUWGtskO6NgDVYVmOY7HA3JHUU1wDRV9P0qv09HoykVcL

kMvDMZIkDYCPKo/q9gCE4ghKliwM709FHvZnrLj60YMU4RjwiMX+92437NCx9mkWslrLQGWsVr/qWsaOqEZHd1Kib4vW5b9t6eRpoYPSTQVxCqhFFNY0nw1UTXi+Ij+nA4ZyB7SoqCwrGFcDPK9+J8rLMWhMKzwq5hPKzCy6TqTteI1KurLpcbKtruZE/VOsOPAK1bIlPZkOmrA4sosLDqn8KOFQwAVokbcTVnoBVmFp/RIBIrOzCiufL3y78t9A

WK+6ShaNsrLGf5+8WCu1l5q+7EQVa84Mma6KXtRVKWiFE/PXegAL8JJPqPKAAL4GUUMS2ET6+lQPRvEW35oxteLocqxscbXG2jl7wwAs/CJGTK/MC4aGi4sldA+TfVS45SdnotP4KXeU3ya6XV7L8bJFkJssbbG5xvcbwKF7kxtCS77ngRzyYm1wrybcHkbd9g93AXALQDGBEgcAFMuVlS6xiwmej8JW3riYSJXmwJlfViUyFXEZeluKIhC8hI0g

zOjJIOqRjmHdt2U8jHZGcSqUMAlL68O1vr8y2KulToYy3Nj5WPYRPrLXpTPl6zy7TS7UTl+qp7Dz4DuMOczww98MWzMZVfTGEbxZ8BIbI8XcvzD3I8Yl9xXiTe1cBH9bwFSBjfoIFJQAgVWBftXszCvPWaXul4Ed6jR83i161kJDKAbVVwEfQqAEC3HWqfnLXBQf7reGkAUlS/WSSB28EAsA2gDGCYQk9tX4U2QoNvBcQ222dtMANje1noQMmYtt

KtEtatuatXEBtuJwW211U7bJ3nttI1BjUdvS+nNriB94z2xdtXblMH9ucZgPvdukAj20Duw7r20tDQQn2583rWOO+LUtdSdUTt4VlcAB0nbMO+DvMAacoADftpR3oUHVQJmnWJxoAAC6pRQYdsNqtttVgECoCv1jO4BC2Bg2yC3Db3Td97MwDXRwBS+FuTL5i7pfh3IYtLrVi1ut5tmp08ZyyMdaAQqyAoA+AVaL62mQxO/rsBCj8KgD0VzTVzXu

B/mQ7WqgIQDc0y7bAXLtddnneJC02tNgBCYDYIBWC7UvO81kSQwkJLCgNudoQB2QxwIHXIwwUFxBdeVkAuCoAaALbu8Mcux1WCI6MMwDKQUuz5CIGiBoHUJZMFatY2ZNuxNvy+7cgN25V6lagDO7ru4nv6AgEKXsu7ge2XtV7hIDAA+Q00FXtl7gJNoCSgLAFACZ70NhbuHVVu2TWx7VYPbusdju9Xuu76uxABj7ru6gD0ABAAXBN70+63sLgATc

zA5eZe3d50k+AIHVLQxNm1nE2Bu0nVw2ZO1TUU78OU2A07lHTzuSSs+2dBsZLO2zteiHOwgDKAv29zsdVN+2xBfQ/O0taC7+fsLv1dpfqLv57Eu04HK+2lhdm5VlNniB2QOucgC9762U2A8ZvXttBcQg+wXvatR9QJ1WouAL50y+d3noCZwY+4BDnw7u4hCsVC+y3su7rFVAfBQ6+5xmb7gdWgcWB8u0i2K7wTSrsnwuDZrt39nALrvX+3JIIA9N

yAPhC+AiEPiBlQgdQIckdQkHVhHUS2UwCSHgh+tYiH8+8wCiA1sXzVd7ijfzWCIYQJTYqN8rfIdM+uhw/XUHqoGL7qH4h9zaB14Bx7D/elh5of5AgEB7CAQ/oFP5ygicPf4H7xO3EAjAqABh1pyaAG/MjNUAGnKUUHsGnKVygADFZgAIPRrlIAANpuNxNe7UPRC9QRIEQAlItclJRvzHAMiTaAd/U+aUU7UPbaRHFcqgBZysRy3g0VLFFJTK+uWr

kecQ+R8wAUWZXNEdxHgAJt+PoiVyAAdQmAAjd5pyUlLPISUgoFxTtHKR2kcZHOQLXIrUiVHhQew+0GxQBQQUIACYqYAD30S3g1d1/sMdBwLAEHB/g4x7RDpHHtTkB31PzeQEcKnEFxAew9ud4dJ1cQDwDG7mdZK25VyvvgeD4RBxgDPQ+0N5yL7Lu3Yd0H5tgwfK+Vh04cYA0Xq4d2Qux9oAsZ0dY4fOHXxxCdV+Dh2VCgnLhxAD+g5uaA0t1OPo

AADcoAAScmlogngEF/von9h4tlwnZTZxCuHf2YAAoBHSf0n33pUdpaJPhcaAA4/EnG0JgTaVyECyT4NH7aHf1FHysK67cnHJwTY41gAFwmgAGR6lR68fPtm2zwdTewUH+BWHurWwC4ogKx9BA1veznvW7qB8AfD7DnaPtl7Cp6qCB1tDaZADorWRpW3HXeEHXH7q1qfuHbVO7TsKA6WUwCbZqgFtaoA9++ztt2P2wrXc7bpzF6BAnpxlTf7+QL/t

5gBfiNvAHQB8R0gHHLagDgHZh9Ad/gsB/AfC5ImTlnIHmp0wfD7StTq2Dd2B7gft2nGQQenWde27sD1ZB7lUUHk+2XspntBy7sb78OYwf6nhe112Ytxzdi3rWHB2rsa7/QAoAKnfB6f5SHQhyodiHmh4ofSHgHHIeznpANOfjnc+4hBqH5JxIfaH+0CYf6HLzZCfzngaETXb+kB+YernGh2VCINNp3Sx+HgABKKgAKfmgAH5GNNt5zkUHsNRWVHJ

PoABkKoAAQKlVyynufptsf7REINCB1SgIABMaYABG1j6JNefR+HI/WlR1JQ+igAGfKUF70fILgADIRsJoAAxihucXZ5OwBeN7f4EJCOAqGmyCnHqrecdmNJ50SefHe0K4fuHmp4L54Xg0Ig3eVAAISPy7+/pA8NpZ8rvvHlZ2pQz7A0AdbjgUoHmAsXlByTY7WrZ6ntUXLGYieoAs4NF6DQB1Qpf7+Y593vxQf5wDsmnqZ9RCaHqp+qceHFkEDVZ

7d7TqcD77Z0Xtc1Je8aeCHpp9JdUtIEeIrb7yvjadd4dLA8fgX2csNwoGgAOIKlFH0eUUMp/7tMNjFwdYsXSJ2udQAoJ3hcknbh6ECGXEUOpWJnHgWWe8XLu4BChXKUDGCz7HgGJf1nzZ/QdSXoDcTYyX4J6Sd/gKl+SBKX61pVcxeal6nv/biUNpdKnul2VD6X4iBqeeHiDSZdMZZl3nvxnBpx53F7TdWPvNX6l8DWmwjlwXDOXoDa5duXKQDee

AA0O4YGFFYADJeuRQfnhWvlpdHLl3NedLjx6bvH1p/qVdBQcl8HvAnFJ2ie+ZEV6edRX8JzRfonZpxSdBnrhzddUXL1+id0XXV09conJUBGgUAxAHJeUXFJ0KDpgAN7RfxX9Fz9d3XZTRwAd7le+VdbQyJzDfMw8NxDfMACV8oCwnv1wwD6QsV2Se3X0V3jfIk6N5jdA1jV49DNXkJ3pegQBl1Df5V2e/3v9X4u4Nfd1Rpy7tjXwJw5eWncde7l6

7Np4qnaA9p9Duw7zp5fsmVr9XAAA3Pp4/scA/p0jnc7Et5JJS3gNzQA/7z7UNvPeIu+Nvxnku6A3JnR5zpfpnMFQgcsASBzQdGXeZx2e+VhZ8NfaAxZ69u1ZqV4QeVnJB9WcQHXNXWdj7jZ2Pstnhrm2cDX1tw51dn61j2fIAfZ4gBcHg58OeRNyvmOfKHy561daHoDfHfIAMh5xDBQRh4ucJ3oh0jeRXgdafVbnzzXK27nsh8YcHnjZxYeRXiDb

YdfHBNzJcInn15DffXYB92g2IfKPXfPXmEKTf03+t23cq3ndzjeg3zAODdN3GN73fuBc16DjXn954+fMAz518evncRx+ffnv517X/ngl5xDAXCgOBeQXjXtBewXcR/BdIX+9yhfoXGF55SVHrJ4AD45thfsXr9Xhd2QhF95wHwJF9o0kBZx2H5mNkIQOCzg3cAJCzgKMJ6CbM9BsDc43V119dGXDF5vcIAzF0w1sXuF5xfJXpts7cVn6V/ERMQAl

7rBCXe0KJfiXft1vvSXl12VfXX8l4pfIkB1e1CzgmAHADbweYONcU3lkFTfKnNN2qcdXZN91c97jN3iC6nVt5ZcwA1lxze2XeIONfmnU1wgAzX1/qBdgXXl0Ny+X/l70eBXcR8r7eVmV4hDhXed4TcZXxN1Sdj3HD1xcpXPFy7foPajyHA5XJAHle+3hV/7dEP4DyQ92QtV9VfCHMDypcMPcp1pfCPLVyqe037D73cA5F2dw+57ep4Hf8Pgj6gCc

3xV9DZiPPN6A077p/lPfxAqADeeAAR9ErXq14AAkSsjbbXJXIACwKmxQewH54ABPugB1pP5FKgB0n33mQYfn2T4U+AAGtoAdd933hYA3nB76oAB8AozCQ+AK/ekApFyH5qt7cp6Bx9Qkkjl48WQNQDXHgdaxePyzT+jXIPGl72dpXru0Gc2IZUFZDDPp1sdYwAwgElBoRqz0rfA2AN5Y8AnklzY+RPPUMQ+nXiN448UP61iDRMArYFADUPLT24+a

XTV54/U3bVz4+NknV1A/jX2p0zfBPLN0HdDXVlyNc2XclSI/2X4beI+SPp/koApPaT5k9dHeT30cfngAJzKf1nNfYXHsGIz91OXn+DtPzAJ0/dPvT01XA55WTUCzgXx1z58o4z18dwPXECxc4v5j3dlzPEl+HeLPVe8s92HuR7LabghDPg/WPhD2c9J3qJ/Y8VX5D5xDKXXxw88Hw/L5IAvP69x48QvXj6w903LdyK//PPD+ZchPDu8NdO7Qjyq+

iP3N05fK+cTzL5KAqAIAAvZvEeAAY9ot4OPoAAA5na8gE0rfzWPya4HZCEvxL2/cqtfT+QF7olgg+iZZoz4XAHPZuSo/igMkAg9rgKeyK9vHxj9Puh04bzdQiXqt1Y+AnRV/48nX5IHJfXPUr+tYJVYN8QCBvaMGm+Kvu1cq+KnHzzq1fPwQD8+JXnD49mBPvDxZd6voLwa/hPnj8a/QvMT9f7mv3Fws+JvgEMXcBIIEWwBbblgsDb/X5IOm9l7A

98c/MZWb8dcXPub4jfnXoDQ3cPXpD2A8w3V19DegnH1/6BvXXd0wA93Gr9f5UXw96Pc7vyN6CfXvqt3Ffj3F7yu843MVyTeI3u70Te37H70+8cPgdYw9dvKrzW/tX3z/+8M3plwC98P7bwI9gvhr4qdmnJr9Ne83V/q5fYyQt1DunblOxfuunmELB0i1HADLdP7L+wGcQAuH+6dkg/gOGeRnPAVrcAHsgTrfi7et9f4G3XNTQcwHfNUxkZnIuWbe

gQFt4lfQfNt5gdedDt4g0JvaD67tu3jAB7eN7Px+Jc+3i70CegNgn8HcK73Z0rsLPTnZJKcHA56+BDnghyOcy+qdxOdJ32d2nfznEkGXdmfJn7u8F3MrUXcGHpdxnf7nvLYedsfx58jc13bd3Ycnv4D43d/vfj6f7gHA9758w3D7+e+/Pyvnc81ocNxO9Bnk3sjB/ZhWRbnE7UwH4cIva15k/5aBT++fFPpT+U80nlT9U9dHdTw0+PNMrdM/rPXr

x09dPvrzo1kXHtoM9PP0Ox7BXH9L5M/wPlXy08GPKD0Y8Sf917ZA/HvL2s8tPRzwVeZvpz9m+rvpAHm+Sv4QDVcyvZUM1/J3/j4B/MPSd6B/1v4H5q8tvOr0C+hPcH0B8IfUL1JAwvZr9PapfiT0k+pPmX8jb2vbFKi/vnGL1i/lfOhwDe4vHgNV9EvtXz0/v3DVQ1/kvoOcG9tfe0NQAq31APF+xfsPkoCQ/yMKgCAAOvI4+gAMnxgAF+KgANHq

gAEPKGBlJQzygAATynRyVzK+CgAABUeP3hSw/bAIGgA3u51AAdyalGTclQo902/C2FP9Cd13yp0xB0/TEAz/UXUoIZ/+PW25hAJflP6G80/XP0DCangEKG/8//j6z9CARvhz+0/7cmpSTek9oBDy/qt7D7E/CgJG+MvKtx98kAPX/M8cvw75Pt2HDWSm/Dfu0FAACv+V+y9KfU33Y+XPpD/m/zfyAEW8j3xAJN4svFb9HDrf3j2w9gfgX9rm7fzN

7LvAvbN/q+jX3byd+TXfb6f4Dv0Npd/JAKT3zb7V9tnedren54ABACewut3gVONV2Qaf+lmpHhx5McBZfr2S+8+ZWZuD22j20L+xf2gBr9wPMkBzUwnI12y/ifHx/vRaw/F4Qybg+XNg2pv9b8dYsZXt2Xtt/0XptyKfy7w9k5vM31c9zfB1fAzF/zAGW/1vvv/KfvPLD58+B/W38H/xZWr0E8qfIL7B+dvET9m9IfEj+d8MNxWag8fHo73mDjvq

AGSB1Ygv0wAxgcPxr/j/Lu3L9G+M/5N9X3nu9xXlJgD3ur9u7ojdWfqG8wAUBQSAHJc//sQAYAaBBCQDBBEbhzVkAdkBsbjDdAqriA5LhzUcAS1kLrkPc1bGgDD6qXZybu483nsB8d/rW89/nmBtvifVQ/oC9w/gd9z/jH9N3lf8rTk1B/2iT5fRKPJsFmpQH9pgZUFoU9oIKfV8drfVRKv80KLpQDHoEKB1AMdYaPtGdtbszApKhDUOqqLd29mZ

guIPIDJAAvVpKhoDKdtCdOQElcubtC9DUPQ0R6goBAAOZGqFww6UlFx+zMER+WPxp8hgLP2LAHyAugOlyqgN9s0T1Neb2zEBMrQkBWjTlqv20A+ugMUBGtyF2dHyI64uzjOagN2aJ+00BkoBMBugP0BmH0dO522YAxgPhqXFzEeFgMjaVgNsB9gMcBnAGcBahhOqSQMp2ngM3AkgG8BnAA0qfgOQ+AQK4AEyQy6H20VauO3M+a2xkBrz0eg220I6

cAFB2GO0h2bgKdOl22ag3tlu2VthR2aOyvAGOz5uC206By226BYQNkBlkAGBu23B2YO3cBx2yqB7gOyB8OymBSOxr8fdBsgqOye24O0x2tsGx2ywMkBwQJuaUlTQ+bwDJ2YwKyBF+3p2Xu1xA3p1Z2vp2JsnOzMab+y+BZdjVuW/iUB/+1iBgB0Y+pfiN+wthP+3dRDuyADDuEdzDeunygAWu37YMYD4Oc10N2fhxN29zReOxVyP+rb11eI+yj+r

t00YpB2CgDO2927mT921/gD2QexD2EcHD2Lvkj2mQGj20ILt2hewT21zWT2wcAeyhAHT2fzyYB8ILMq7N3L21zXEute3QeqAAb2P/3ZB+gC0BnexFBkH21eYf25BrAI+OE+yn2Vexyu8+zk+dvzb2YQFX2GbxOewr37eqzQf81pwFucQGFuWHwOBOHw4uP7xSgRHz9Oz+1f2ZH2dBn+w/eQvjBBUQL/2MQOkCcQK5BvDC4urHxgA7HzTOnHwCy3H

yzO5txQOgnwLOwn3tum4BwOjtzv+fXw+OUnxrOnt0NB3t0NuTZ3t+s/zhBbb1Y6iIORBBXVV2kdzRB+n14OsdxTuSh2ceudysOZn3Tuc5ys+cdybBNn2Rudnx0ORNW3OJdw5+Zdxc+gNUruu7y8+gVB8+mjy3efPz0ewfxxBzjiN2ARyCOIRzCOERzaOCRySOjXgOOPynL+WRxyOeRwKOgpzOgJR0rk5R0zklR2qOtR1Aa9RyPBzRxa4m4IJ+fRw

GOQxxfkgcFGOu4KOOmR3K4Mx3OgcADmOXxwWOSx3JAaxw2Oyvm2O1EE8g+x30gpfz3Bxxwr+9X39eX9w2or8kuOEzzBsOIPuOB1wJBZu1AaXf0rOdh3zBrezruAAItBQALFezv0hOgoGhOU/xoal138+g9z3e/n0xOUjwmauJwJOVF3xuoX1BOsV1pO9JwqeqACZOLJ3ZOnJxbs3JykovJ3vBJ4LYg9thFOYkPxsEp2lOyj3wh6wKO+qoBA+db3o

BB/x6uAWT6uzAM1BMHzCeF/2OunAJQ+sNjQ+dLHtBmQPP2LpyDOHpxaeiEDdB/wI9BpH3I+wZ2f2jkIX+foMR2ufk1ufAXo+Y2yYO4YLbuPt2jBq1nWscYMQOfH0TBZYOTBRZzTBJZ0MeQ736+VZ2k+5B2IhVB0LBZoKXegAOl2ZYNU+rB3U+7ByrBOnzWMdYNQBiDWM+id1bBXYJnOZd0s+Gd2s+id1s+2Fwc+O52HBznxMObn0jBHn2rugdQvO

Ru1vOD5zzAT5xfOfFTfOX5x/OqkL6BlkDUe2913uyFxgucF0QuyFzQumF0ae3oKIgT91GhxF1++lfz0a62zveA33wAEX0be1ADUeDLwQeW0JgAsIIIh6D34upj2Eu9bzG+xYLyh8WXn+s3yCgTj1qu2QUEOm/yreGkJoBm320hL7xD+aoOP+BUNP+xkPYBl/17e/gMtBs11cu7l1QAnl28uflwCuQVwZBIVxgeqAA0eX720eLoIQAZ0KxuTdU7+9

/0rOpj2yueL1ehBD1Ee03y+hVVxueyAF+h9V3jeakP9+ar18eYMMP+ooKhhkfw7e0fyNesfwtOCMIT+u12RhC12Wua1w2u75y2uO1yRhksIuAOEOeOeEMveDMPXeMAKuuTENROLEKIBMNyPePEPABZ73nBPMJKuIN3+uj7x1hf12LeJMKwBvEM4AaN0/ex0NhuTsIC+ZsKieFJ3feuj1vekV2/ePoJ9hkD0beAHw5h2/w2+WkIbepMILufMNJBhp

3JB8HzsuHAPhhzQN32fNyLotx0Fu1kNFuOHyVufdGluvwNlu8tzV+ZH1zhKt2/2/oL8h0QIChkIIY+wUMTOEYKjBEBwihcBxNumZ2ihOZ08OSYJUqCUKiASUN6+KUOzBlIPduGUOb2dvwU+433NBAd32+nZzU+odw0+HLy0+1YMLgtYJjuVUO7BNUKnOdUKEO7YMzu852ahud1ahr303OA4NHeqjSMOo4NMOhYKrut10nBE72nBBMN5+p0NNhkXz

7ugVFF+RsKAoEAPdhL8JY+/d2p+H8PC+z8ODhVoJl8CTxnuI0KIuC9z2gS9xbwK92mhTt1mhWDzYgQFyJ+O9wguS0MPuLeGPua0PPul9ziON902hD930gO0KIuJLz++99XIuk9h/uf9wAeQDxAeyBgfhED2buUDwuhMDyuh992v2SDwauFMIehmDyehuD2IAtMKFe9MKd+a7xd+S/wTuZ0GoetDxpsAMKoB1b2BhEcIYBAOWJBe3xYBRkMO+JkLn

+ZkNieqCLRhcjwxhijyxhp/lUeuMPxhLsO9hxMKARpMLuhPCNd2VMIpyNMMFeE33Ihc/w1h4iO+hzMPagg0Fce3CMQRnMN3+6rx/hKiJjh08LJBgsPBex3xFeTQOv+uiNv+oCNcuwZmu+iLyyeyL3yeXxyKeJTxlhBXyK+75xqe9T0aeMz1ae3rx++pLz0aTX3WeNLzGeGEOCujLy6+szz8RA8MrOyz2G+RSNQAmz22eeAAEkez3CAmDxVuQiJcR

IiOABVEIleniILeyAGi+Dz2W+ciMpuYcID+QSOARO3whhJILCRccIiRCcMhe0SJ0RiMLYhGXwyeyNjSRj32e+rl2xeXxwN++LzaeNXzIRB0P6e1uUpe1L1DedLz2gV0OZeeL1sRWYMrO7/2CgPL1We1v1t+OUId+AOU+hi/zGRbvypee0Fle8rxmRTDzmRXMKD+HsJl8qiI1Bcewj+4oPjh6kM2RcMNO+8fxl8ifzWa1rxdeTrxdejT09eBLyuRd

Xw/uAP3bkgb12wwP0eR4bxb+dSLeyVsAFB8WXuhSbyneA92eheYGcRk8NsewyLERDjwkR7v0thpbwER0KIxRqr0CR3MOCR4MN6uUH35haKPWRkqJ7e2KLFhuKJARyUJN+qUMf+OXkCoyb2ves73EuC7wnhuUNcRwtmBRpDw3e6sL8+272thj8OpO+sMPeX8I/hR7yDhUcKdRNsM9+QNxdhgCO/hiyJtRMN0sRPqL9hhMIDhViP9RHqJmhSr3kRQM

PDhdAMjhWp1CR6iPCRZ/yFhUSMve2yJ8Cx1TQ+UwCzh2HzsheH0o+MYGch0NiLhXEEDORaII+5cN8hXtX8hMZ3jOcZ3F2IUMCoYUKbhXH1bhPH2zO/HyxuXcOhqPcPTBYnzsRxByHh6UNrOmUJk+7HzIhU8JTRhUOgaxUPdaC8J662n37O5UNXh+8Pn2tUMbB9UIzujUIUOW8Jzuqh17BbUJPhjn06hTAAvhPUJoO18KsOt8JjgL0EYR/n3dRQNW

C+/8JnBFsNth1iKBq0X2F+XyOF+SX01Rwtku+6Xxu++yOy+GSNy+WSPWuOSNQAVTzyRJXwKRR8JwubSLJR332uRSEKr+FSJaeTT2peNSOxhTKKKR7yKaR6DyIh00FaR6zwGRfKOiR7iKFRoKOleEKKW+6zwlRASNoBCyKjR/jyRRBkJRRWoMiRicKxRcf3VRwtg0g8TyJ2UwAeOySNu+93yORmLxORSGP1+LLy++Pr32hGGL0adyOB+HsDB+ANwh

+Df2Rg0P3chv6MR+qP0x+2Pzx+BPyJ+pP1x+5Px0xlPxC+xsHF+PPzLhDYIF+0NlZ+04Img9mMl+DqKcxzmL/Rjf3fhdmOV+3P08x0v28xzmIQBYv0CxJAWLhGvwDq2v11+LF3kxbyPJhHyNlB5v05Alv1+R8r15RZqKGRlEMFRoyKZh4yI9+AN29+eL2YxsKOlR8KNlRvMOWRaiMMhqaJhhwsK2RycNiRiMIu+omIWuyT1X+Gfyz+ufw4Cv8IL+

WdSL+drWYAcEO/BJx3IRn9yX8NfyBgrrnr+bMEb+zfw6+rf0Pq7fzjeq32HRk+y7onu1Os/f1Os3tWCg3KOIAo/2i8CoMn+QUGn+pqMBRbiNERC/w8RhWLd+K/xGx6/3oejSMresaJ0u8yJlRAaJCRtWORRQ+1RR8lQlBWiI+hWaI1R8SMHe2qIf+gtSf+gVFf+OXiDOn/zi+RvgnR4WOnRnqLBOIyNABGOKPedkCgBfKBgBMWMgB1mKb+JACQBe

oFQBpD3QBFOPthgEAIBeAMPqBANpxMxFBspAPsa5AKHR/iIqxrGO+x7GNP8nGLFBQOPRRIOJKuYONQAbWXk66chUyfAIEBTECEBGBhEBgQP5qDwN6aZ+3W2akIiB7Lx6qEIODBUINUB0vnUBr9WSB2gLSB7DQMBRuKMBdJFMBScNO+BQK18DlRsBdgK9EDgKcBOPhcBegTeBTYBqB6gHqBHAEaBOiKWg4gLuBIQI1aCtXCBtQMiBlcMDB1cN1xtc

IaBBuMSBDp2NxqQNqB6QP2B4wKtxZMLMBtuLbglgLWaxQOdxpQI4A5QJp86eKyB3uLqBMkB8BsNhiRh0A9yUXUAUKrgxyHJhPC2OTi6qm10WSXQ02Bix4smdiqaXsgy8SwOvqXQIBBN2zUhmwJB22wJGBrgItxBwImB121rRFtlOBswMuBuwMWBtXRy8KuNWBYePHxQO0GBwwKuBkOzLxTYHnxCO2mBZwIe2q+MO21wPQgtwOHxKwIeBhOwFuLwP

J22cNp2nwO0+3u1LRVEFHxFaK9BwIOo+AYKjOOuNG2X3lDBk21AOIr2JsguPOqs8KRB88JRBE+0122uyxBTmJxBfBBVh2VSlajAL+xXGIBxPGPQeOYOpBwIJ929IJMRr1kD2f4GtRp/lD2MkAj2gJE5BfD15BIYH5BbLyFBGe1T2AuMVRQuOVRf1xDA0oO7+8oInRbew72uIFVB8qPVBeBPQOGiM7e/O1+OMNkn2+oMQgwhOX2JoOd4AKJLBu+xA

Re+yT+toIw+x+JYAToKIRRMJ+BD+2I+noKv2uLyJhgBKjxwBKDBoBIh84BM4AzHyC+oUOyh4UI7Rd7VNu3aNihpIPihdt1E+IcJSxkn1HRuYNk+o8ILB7n2gO6OOU+3BNgJRULnhJUMXhZUO4OBn1CxwtmqhLYM3h26O3hFnyzuB6ObBR6PzuJ6N5ag4KFq56PLurn3HBnnxsO3n3Z+76NtRc4MjRXhywhy4K9EgR1QAwRxtu64K+OpR0qOiFESO

yR1ghExwQhB4L5OTR0KOxR1dcpRwvBV4J0ydR3GJAp0fBFckqOz4P6Ogxxnk2x0/BwxLL+oxN/B2x0Ahe0GAhLGTAhmx1P8kEN2OdkC/B5fzKR/TwuOD9TwxNTRtO2EPxBqsKOueB02xpGPkJ/xyuxmhM9ht2LwBNEPb+WsMYhH8P3e/2The7EPxOhJwpO3EIaJKN1/e/EIEhjJziOzJzZOop3EhFch5OixOaOUxOwa8kLFOPoilOxiI+JXOOoB8

aLYxSaNwJMBOE+bAKaxmaJaxXAIshAtyshGQLfxl+3shIZ0chphL+BZaNchCtzI+nJM8hW1m8h6t1sJtHxjxDhLL8ThITO+fwnebaONuXhLbhvHw7hltzih3cICJiUIzBpJOIxIRLEAw8PHRERIbO2UJiJ1/hpJFYIQJpUNXRqRPrBa8JI6Jny3R/BybBO8L3RC5wKJPYOKJcmNPRHUNdJl6OqJ/UIlhB+0vOqAGGhc9ygRUoBgRcCLXu72Meg80

L0R6CNPuy0KPuq0NPu60KwuSGI4RVhLYgJCJfuFKP++yEKX8f2xdhTCOfeLCMuhHXyZRN0KIxUOL4ufCNxhR2MoxOWP5ReWLuxtGIexylyCybMI2xZJIURFJN5xxly4e1JLiJtJPTRfGKBRYuK0JisNuOKMP0R8j0xhKkPwxpj3MRoaNxu1hK/RWePZhwRLDRgFwOs1MNyu2WOuxH0JoxBWNIAP0M7J/0Lexfv25xIMMTRzPy4JscOhhmiNhh45M

ZJN/2v8OIJrYS11Kem12yegZOnJ7wEwJ2dUJBgaJbJZ1xBJdqLBJesM3ep7zuxrqK/hz6PohQ90thIaK0eQEGQp65JgBqN23gCN19hqFKwppABwpCFJgBwaOdhK5JIpzRM5xMaNmR5JK+xVWJ+xiKOTR9WLWRaaN4xmKNMhr5K18acLQ+oOHzRjoJdOpcPzhZhLlu/JOLhCgAEpqtwF2QBIlJDaJDBTBxcJMvgbhcaMVJXH2VJPhNzO6pP7RmpN7

h2pMhx5Z0Hh+pLHReYKNJWUKiJRYLphyvnNJcBMrByROtJ0dzSJdpKXOWRPXOORJW2eRL3h7pJahx6K9JpRNPhhhz3O3UP9JN8NqJU4PqJj6O3eRFLlJ7dyyA9qLdRzCPopwtlfRFyIfhfqIipEOKAxiSKGhs912h4ZPwAkZKmh0ZOjgcZKxOaCL3uB9xWhJ9z6OaZLwRLeAIRGZMQeZ0BzJe0NuJlCK4g1CP/ugD2AeoD2LJT6LippMNYR2D04g

7CPqpbEGrJelNrJp1n4Rab0bJh5JApzhxABrv0oe+kGkRdD0QBl5K3+NFLhR+/wRRwtnvJqyMfJdJIzRL5LVRKcPFhxVNnJhiKUerrxMROMIGp6jw/h5FIQpo1M5eq5OQRu5McR+5Lt+5lOoxAJJBR7ZMkRr1M4gviM3JVFJhRG1MqxW1OqxOBIkJkMIfJAsJYpGyLdJzWOOprWNOpf5IP2SSL2RWXzSROXzy+2SMEhcGPyRZXzde+0HqRxSPJRy

mMpRBZIGeQzxaeVSMLgjxJl8Uzyae6zyeppvxaRqzzaRHSKEAOz26Rp1n2e/SIPJfxJl8lqLbJp5OZhkyMYxzzzWpgMM+xm1NBhkNP5xjFO4xMhNHJbFO0RHFJ2RkJMxpByNyeD316O6LxkxNp1ORe0HORimNKRk2KpRdyPBRUoDppTyKlALyLORLL1ZpqUK+R96P2gVvyyxn1OERzZLmpWOIWpC3wYxUADleNvwVe0tI+xUqJ5xdFL5xcqL0hCq

NhpSqPhpKqJFhZ3ziRqCIJR9ryJR9rxJRex0uRaGLzJFCI9sNKMqQdKNpeDKOWxTKNjerKIQRupK5enKOp+R2MFp70NmpmOPyxZDzoxhb1FRcABexq1OBpMZNBpvZNopENPipUNNjpkhJpJwOOfJ6tORpM12jJY1PQeuqOf+BqJnepADneLuxNRb0PNR5sJ+pVqPApTRPtR+7xxxLqPhJzqJNhFFJgBfqPtRKVN6pzOPIp9qIep19MopfdMlRmkI

TRyiIYpQ5PjpPBMTpIuKieYuJ0JuhIzheaLZJBaI5JVaP8A3+NQA5aMrRFH2rRatwjOUlOUBgULAJclMgJA2PlJ7hPbRsYM7R8YJih6lL8JGpNBeqYO0pj9LnpepNIOMnwVB48I3pM6KYpKDQtJSROXRS8Kjuen3XRHlKcpK31HOzpLcpnYJcphRJXOXlOJp07x8pZ6N9JAVKvhE4OCpd8NCp3VPCpD9MkZKbwAR6FLPpUX2sxvmMS+MNjSp6VKT

qaXwkxYGJxpUGLKe+NOK+JXFK+hSKq+qGKUxzVMa+NNJa+uGPa+8WNJpztI+OXxPIxo30bpm9IohvtNbp/tJZhi30eeTGLDp1FIHpctNvJ4hNHpMNL2pcNMaxh1KnpAmJOp4OJEx2jKu+2tKkx+tKe+htNuObUPe+CmIsZ5tJuR5ATUxANxB+UoE0xx2Ip+emIp+CP2R+6Pyx+OP3x+CsKkeFmKsxC2ORgVPwuR7mMixDmKZ+412JsrmPqJHTPp+

nmLROzP16ZJOP8xAzKCxnhyl+fKBl+AOXCxiv3F+qvz/xROK1+Ov1qRCWOyZSWLexpDKr2aWJy8A9w9pIdPcZuWK8ZrZJPJTj2KxXv3epZOMCZ/dLjRg9Plpw9MVpH9MiZCdOiZY5NiZosPiZQmMAx6cPwqMZlT+I2J6xOfzz+r8Inehfz/Aq/zGxNxItpVNN4gM2Lr+CAMZR52PJAJe2SxNdK2xvUB2xSUD6Q+2Ndcw/zzAJ2NugE6JRZqO1Egp

pKBRx5Lbpf1LTuGdFX+3dPKxYNMjpQ9OjpNWOhpKyNnR+1NVpiNP4xXzJRp4ONnpz1IXpcOLYAb/0RxsXy22KOOMpajMp+3/wpZwtKpZ1BIVZON1xxf4HxxWQEJxcAOJxLTNlZNzOgpONwwBlON3O9jUNZzOPpxbOOyqTOMQpYXxIBVOLIBXNgoBPZPuZITLfpO1KVp+BJVprFJ5Z7FOnp5kPQgPAJlxggIw6wgNEBbniCBweIlqT9V6BINOneCg

K1x9aJUB8eNpar+KMBKQMyypuMRqKbLnxmeLyBDlztxk5KKBTuJdxZQLdxFQMGanuI8BXgKrxDQN8BAeNtgQePvxkgNCBO+MQRmuPBB9hNjOOtwSBdmQMJ2QLTZOgNTxZuOAZ2bJMBG5IZJOeNyQeeM5aBeOLZxeNLZpeKTx1QKrZwT39xHFKWgc3UMMq9j9y22kMsGw0CC9pnW6Tpk26CKx3QxcDkYeMCyg+ACJAA4GEgmAEwA0wAigCQE2Yl6D

TQMoFjwnm3uG3my+Ap6jjIHSjxEmDiC2jw3OQiQHEcvziwwEWy7aREQ7WSwAcKbeWOwwMnrKjhQpYd61wcqW13KbKRmWmW2bm5U1bmIYwlWdQ2na2PSvKE/RqmlcTK28g02Yuy3cU9E32wvwzdUTW2eAQw2jKGqTbc5xExCHW3nSpgxAqvWzhgCIAAG1G1EmW4xbKwozAGO6HoAVQGwAiwD3QWUC9M3cB4AVQBaAMAAbgxcEtkhEHwAC6zfylziA

SKIQxYAKFb0HwCo4xUgDi1KwNG9kHtQME12QR7ikIrE0TIfhD6CSIFzGHqhLAh4ESmHHFwSYSHwSzIR4iCI3GWMSnLCgqx76GE2w5yy1w54q1PKkq3HyRHNnaOs1K2fc3kG27kNmsqVdYaq0zw4wzTSwy3aURSgnS1SAgwFHFNSc8xPyJg1TKtEz+kvZQucNQB4ANQCJAnoHgAXZlBW3+SsEPHL8MQWGrG4rlrG8vFs2dJX9m9mgq5VXJq5cAC7M

Ec105IRnwQTLh2U+wD6YuIRVQIU2byg+mxElHBoikmDOAhwGGcAalBc0SUS20PV8534n850y0R6isxFW76xy2iyzy2OHIK2DQzWW2sw2WsXK2WDUyc0g8wVStyCOAVtE0E48zPc2Y16mPSXamROA45C8zI2jXMDIuQn/y5iXXmtGycoDwOlsEPMUWWTUxIOTU0WSm0gUf/hWSF4TU2XePQAN4Qz6qXTE5EnKk5MnIoAcnIU5SnJU53cDU5GnJ02y

dCh5sSx0s8Sy3ZVmwTaySxOcqS13s6S0+SmMxqAwkE0AkgD7wyQHogVQFwA3JHcGA4Abg+gGUAnoFwgHAG5IfeAqWYpQjCqhHuOMMHTCdS3FmX0QxomqWAEXLA9CpGGokyUnzS2NBsUtfXtQgEwLmHmCDIRQl6McZEPA+HDoKks2rm25X7a6HNtKQqyw5E7nVm2IxO5+HLDGhHKK2V3JK2+PW7S8g0KCyq2umLrCX6lSQMQiQCxUHwCHCGoieK1s

24AVwEz499GuWRgyTKKGy5GtZVeQiCQMKns0AKzZV9mgfRPZlQBskmgE2Y9ECUYeMAEgCYGYAHAAPg38QHAMoBkkVQBNoIpQCGjqleA1SyDiTLHfS9xVgScfHwQGhFv0iOlLAUU2JozeQSMl6VdGrnKxkbQWvEriQuIDEW85Vc225fbRn0cswbmi+hrSYYzd5n63bmBHM7mUXO7mePSn6BPXkGmgCo5ofKwy2Mk3I5GHbiVuBuQo4Qwwwqm3ayfN

5cRXPHinmxmcR0VwgXIG7gmADiqFI3q5ABgz5nEUimIPJo2efM65O4ynWOWG/5RIF/5//JPSbfOo0QQgYiFyBdWNQWZmL3RhEkfL6UagiimMPNBAqRi5WpYSoE9vNRGe5X25r62C5rvJVmO/PBKe/J/WUJUjGMJWjGcqxJGQG39oPAGwAD3NXy4MFvoe4AbcIjmeAsCC5iQLnDU+XMLGKfIAqcbD4mGfLRUpUjAFgnOS8TlBTkJPkAALqaNeSigH

eTAxSUbQUYGQAC4SoABpzRMogADfTKSiI+Mr5Zsp05sUYdlOnNOSAAI3TMLhh1pbKoKNBVoL9vDoK9BUYLTBeYLLBRWzmADYLe2Q4KnBV6IJNvRZouvDy1jMslzwnuFO8fjkNkga5MeVpsi+fRAS+WXyEABXyq+TXy6+Q3zCAE3yHXP3jk6K4LNBXoLdBR4KDBcYKzBRYLoIFYKsgYELF2Y6DHBRhdnBdG0aeT65/cgzy/QkHlD2cG5C+YjwpsDK

MRgDKBDXAmBPQE/EjAO5s7sJ6AJgL2kW+dTMIwriJvVhQN4+IIRnONGlkVPitgyNcA4ZJ7EAXJ0t4yOEJ4zGPNjeWuQH0imwvtLIRqOChzcpvD09ufwMDuVls6htvzMXDBIzyl7ytZsRziJqRyu0tXEyRneFqlEmNKyhfznyiqgQBOBMIyixNMhnHyd8onw0CoaspBRyMutmaE0NugAKACvAZ4EpyEgE0AeAN3AD4AgBnACvSWgCMBA5kYADZkol

wPLbJP+qNNTpL1shrG8grVhALGefCtROZUAiQFIQVOabxyxHAA6gNkAJgJswEAJswD4LhAzgNLyM+g4gGgsXMk+Ye09kLex1hTeIYpt4porDeIARn4RgnHpzjCAYx4MKR5Cwoxw7uoGp4+BbzwOVlMtucltYercK1+Zhym5i7yx2rQKXheWoGBZFzveZ8KYuX7zfhRRNj0olySucCKh5qMATRiFEzgBoIb0sxzXOFik0kGW5fuW/zFEmmUJACjBn

9swBpgC0A4qjUADkHugYABQB9ACMBSAIsAKubMLyRd55d4nLEv+v9zLRMYl1gAchjxAyLx1sJz7BmiKPNJiLsRbiL8RYSLiRUCEyRZ5sR1hGEQlI/A9BH3EFhGE4IZHghtUEcADInPxrOUmYwdCSkgyBhg0zL5Zz1tEggBHZwiNAIRuwGIlK5p6NTRUiNzRWlt5ZhykHhdQKbRR+s7RcJoHRYVsPhdFzrua6K58mSNzepVtuhhIV6Jgfl+HO+VTl

jvkTitCLOOEx4QRhGKnZlxyetoGQ6RYQlhJjWNvZmUAZpkso7VnKQHVl4JiEJBhJxb0oZxVoQKorEMUgFMxewMuLd+gGslesdNg1vpNv1NOMB1pdM32Pmt2hE1EBhUSAhhSMKxhcoAJhTMUsoNMLcxfUVLehHo0imms61ksRM1g+xs1ioN4ZkOslxjKp+1kMUUZmOt1hkKMQQEiRGALXz/CswANQOoBAKpsNMljugBwNgBhIC+yOAJegG4AkBmAE

SBMAFUAUYN3BwaMoAZQJ6B7WHcMc8oENeAPsAJymlZ8cAChTRnYoiwkeA+lDiJdRIaFbOfgV44uSxAmMmRU5pBNUjJ4oziEjon4Lcht+MaKfORuKUJluKHeYO0MtlaLR2jXxbRbiM3hfvynReeLfecfz/eWSMCNk2k6XCqtF+vRNI1EtJRev0Z1pDoM+dOGk0rFglJBa/yfxdSLAvJcAAjEGRKxSJKJ1gXyWRRIAeRSpLXpPoAl2FPB9AN3RxQLh

ALgMoBL0JIBcIET05hZUtLuiByUzCgJ9BH5FW2MEZxgGhg/gF6wnJY1KM5ojgRZkTgJdMbo28sW5hnMYx8wjYwChj20pZlQIShlFKyhruKqBdaL4pYeLEpRFzTxZIMfeT3MbuSfyyRs3yg+UbNgyilzfRUaV4QLuAtVs8A92h9zwsGA4BCJhhvxUiKdZCiKIALGLlAPGLExaQBkxY5I0xRmKsxTmLgVhB4qRaatQKmWLS5vxyRJkANGRV0LJ1qG4

d0PDLEZUmKUxWjLMxdmKeAAxKgyjvFHEqAl34FiJriMrEQyEGLTOc45UMMEpvVAThshNcUHcGzLwpIikY+KywbOYQLttCDh8EIrzQlM8NF+euK04maKURhxgMOZQLneXFKmRM8KHpSeKLuX+tBCmwLyOWSNmmF9KkuZIUw+S6A7OCEg3xS+LFUictJ5mOp/GPgN+lFDK0+bIKAeTyxyxZutWuerprBhAYwJQbo5hJ0VSNi2MusCcADgGLLa3C30j

ShVFZZZ8hXgArLb6DDMuimHLFetUIg1hONcJaGs52LdMtVCJBL0F1KepX1LeqoNLhpaNLxpRb1PpsxLmiqxK7evWsuWLkJjsL5YreXNE/DJcADkB0Eo4nsBnermt8JcRL/dDug2RccAORQJAuRTyKOAHyKBRUKKRRUmtxot9NU1iVEZoisoHer2seJQMU+JUjNBJaOs/es1LhOWJK2ABJKSAKOxpJWoAuSjeR5JX0L0AHjAOALiKEwNvBzxllA7Q

hMA90H3hNmBcBI3FUBbhh+yzJY6poQD6p/7BEYrdAjQMBXfA2ZX2ZH+UBhteYCNxCDMBvJZOFYEOOFIhikZmGHowvtG+I2Bv8AIcKMsbecvzihj6MLRZrLYpUGMFgmFyv1klLGBQSMWBSRyYxnFyyRiZLbxblLlBvssOXBTQuWBCKrcHWASQtlzeALuBoyLT14RdVLoZc7NuOfiwpCATKmpcPE92dakFJbmgFipIB9AMXBjgAJBpgLOAZQMXBMAH

uhquYtgy0h4ZTJVgN/5TW1c+mmlxgHGRHioSxx1AcBYBCzEgrLmMgJlmFDGJlJJdMiA5xRZKQnNHFgdHA52tqFKl+eFLfioQrtxevyqwsj0nhQlK1Zjkp3hc9LnRReL0pW6K7uWZxPRUCL6Jj5FGWGSxhmFRoH+T5EBCGsKqpe0lIxQ8t0PAJBvOIsA1DgmAnNLhAZ4LgA+8CjAfgPRBIBhhgsZZSLCxbVKjEuIqfZYTLgJTNsbTNWLMZjPAqgBc

AZQIoxFgJIALgBQBNmEIBJABMAWgNgBpJQOBkgK/YJpTLzLuo4pySEWBgMCuk0jD/BS5tHwEQEFZzgriV1SruAIRBEIcUokN1pXqU8pELMb6Ek40cPqFreadLbecykLpeQKNZfcKbpdrL5glWY8OeFz9ZRGNLuTEq0pa0MTZRRNWgebKvRfRMhlqoRUzBoJ2lu+LiWDbRhCO7KZBSf0NmDuhilcwBSlaQBylVZhKldUralccB6lX3hGla/kECsRt

2xPokdCqWKJFRWLFBcTKqxfnyROd1ycsBiqsVTiq8VTUq6lQ0qpUu2KvJuKLUpOGlCEG4xTRvnM/eEtJQ1FFhorEDMmRhtLpMDR5UkOAJd+m3k9kK45JgEYxriIThEJngr/FQQq8pncKCplrLSFd8ryFbvzPeclKzxYfySJmRz6FRRNpREwrg+W05B0sDgrFNwrHZQl1Iys+KnZevxPOL0ZwxQVzjBjVLcZdSqOlVIqjJEHLrCsypQ5UWLw5Z0BC

EHKqZCDqgK2quKlpmMBMRAA5ReqjJQrL3LpJo6stiFcB/4JiFq2FYoZResJjunsUo+eqrAjLZxMJZnLxxrFFTpgZMiJXnLw1iTAFFUoqVFWoqNFVoqdFX0BjgPorGJTXLCoixLl5W0VHelkVKog7odIv3KrpoPLgSP0rBlcMrRleMrJldMrZlU0B5lYsrq5cmtF5Q1J65RXB7ekR4E+asIwlCXkDlGVFeZqqJatsdg/gOvKlorxKzJgJKOxbSRUZ

iTL9oofLj5VJKZJRfKD+FfK2pegBL0Jsx9AHUB9AFpKKRkNzTFO2w0MJE4PgMggAEJYrxgI31DlkRkEHLCI68oCANucQKmUudLAlZdL0ttdLDVaKsyFblsPefltMegbKqpv+sxUoBsFVmSNSmgCLVND2EPWEThg+LUgGOfRw4VcGLNpK8gzgsYQpwuRk5EiIrfxSfwmueatOlW1yQJQjzk6AXjpbNJroeeEK3/C3i8mojyVNjosEusU0CcoKZYbt

xYM7MuRyeZUBZNVTy1tPuz2hTuyXkoyrOGGksPkm2Jr5Q84agFZhMAAkBkxQ5qY8sXB9ALOBcIJswjAPoBcIE0BFBksqxRZURIcOAlHFFhgqNCaM4RE7gLxF8A4YrfpwJsGpW9FbR3kFAkuIgaE28p4pDGJIQXOeg5wRr4rlZSWlNxWrK0RsQqgubdKdZeEqllurMolcwKiJi6K4lVeKKJv2rspQqE7xSHyaOdbQk5cKojIpwrmtsAg8IhbzkVXL

pUVdGL0eYpyZQJehcAHjBasN3BNmNMB6ANMA+SBANL0NVhGnCVyQVjjLF5ts5glMxrQ1TZMWpUyroBTuh9AKNrxtZNrcANNrZtfNrJAItrlted1cVvCkk5VHKApV1YfIlT0nukRl4Fc6oV0vm5PghnNW9HiJfnO/A8BI3lVynsA0MP2FYpDkJXxNcLVZc+t8NSQrCNcariNb8qzVVQqu5tVM6FbdzgNrELCYjlKHVdzpfpTTQQcK1sgZdAhHul6q

D2m3EUBEIr8lYGqNtYF44QAxJUNX7LvQne5w1TGqViFGry2F4JicDFMFgIiBAdRRoJBUtNSwGDrS5hepDiKcAa1b4U61QEUewkEU81s2qC1pUBMAHZqHNU5qEgC5q3NR5qvNT5q/NVuqF5TWs91aVEx1WvKuJU+xp1U2rWhGEUJAJgBNmGv89ALIwagM4AKAEYAG4GyUEwDUBtFbOA9dQOrt1YbqR1Y2MMVFms05dxK71ZvKH1QjNkZrvKdovvLG

VfYMZ4AOAEgEYAuSK5JjgFUB6ALhArMAgAagDPAVgJsw8YHUBRReZKJwnSx9BPjRyaLZLvYqrykEvAqNyL6QYtmYkaVn4R1yL44dRs0pQ+L5LmGAZz4gOW4d+t5h+xblruVqhy7wurLHeYFyhPAjrskiRqzuWRr/lYbKiRsbKbVXdy8gEkqmZdRyCdYql/SEyxsiu6rSCCgIuYlvrWlrqUjQvxqj+rTrixbFordGcAxNf7L2ubCsmRbIqbNdyQ90

MXAOAOsUOAAZACRTAAhouSBZwNVgG4EMLAEk7xHHO9pW9PEJTcAIQr3P5ZoJpuJbkIkIulgchaNKbyHOY7hn0nsIp+cwx3OYyE7FUk5odX5yyEgFz0JhPqjuURr3ecjrSNRrMJBtVritq9LLxb6UyRh2pV9f6lLZZfzzgpcgaPLCqQZT1N1+PzrXRnW4BtTZ5iuR/yVEuh49oLOBXgBUBABQYlwVogk1gENYdtRrE9tfYMxDRIaHxsoleyt5MzFM

EMlgKxxfSMRh/LMlMdUB7RbxIwMkhktz0tatzXogWEThUEhNuWFKVZfgbEkhsVcNTuLG5iVrPleJETyhQrHpeRqZVkbKANvKsKlMBtfTParwNh2BUdCch+mBoIAxe+Lx1N5gvgIIL5mKfrixh7LRFTIaDGJ5hKSjnymyj+0vZJTyeNtuEKeRGywhQpssclot+tMjzsdQ5x1NQkKtkumJicmSoX9W/rjgB/qBIF/qf9aQA/9QAadlpckZtODzija0

LN2SZrTDDZsH9Wt1LNWm17BsVh6IMcBZwBwBZwDeK19V5scNHW54EMsITgOYreWN4lq9YUIhVBW1O9PsoVRY8gTGIiJg+CVIakHFtmGGuU7DX4qHDYBlV+UErLRe4ajVVPryDTPrKDdKsoxrQrF9Zjr/aMkAB5mBsN2h5g8aIMxlZMMw7Je+LpEhBgU4v6rU+SiqhNT/1mWAnwVhgJz6VasYUvAbtriQhD3BYwZyZjsT4ISUh/qURBoWQhCWum8B

AAHNygNkAA4aaAAN7k05ANwfrDiS8SRWBKKAQCwfmrY27Gd5JQMoBunt14iQOKAeTWyA7IFybwQOhjKaU1VfbIB8CAcKaBTd09QTsybT6aCdi4KtZHUZwENcVzYZTYKbSAPKahTj610TqCde4KziMTmDY4gBNx4GEwd/fNQAHBYVoJgI15quL9xYbHmxSQH3hRAubZAfByBCAFDVabPAxOAP75mbH2A2TWPY68Zl0U5I15MDCT4MtFT4pKCpkT5o

AB+v3dEsCzuMgAGolfAyAAEBVAAFeBd3BQCcFyx8+Wl9EkFny0mBn0yUlFHkw8krkDFCCukEKxNhJvfu3iLfkgH2uO0vnGxL6nd8Ot1WQvtgkoIkB9222Q7N74M8g41VrZyYK0oOGL2gxTPwAmvgP2gAGZXJihNeariIUQADK+rpk/FtJQDMo8ZQzdVxAAN3Kh4MaOd/QJs05rguBmRxJXJxWJcR2LN0EFYUmgCX8dZsuOnXhoW1ABCO1AE6841V

Egs8ikogAHT9Nby2AqvCAAfKVKKNNUUWtNAYMagBZ5PabpuFJQhuARVbzBUcVIbnRKKPGb8DJgEJ3hea7IE6b8AE/I1KFKTqAqv93oHbV5WhearzTA8uILeaE/DtkHzU+as6hpVvKgBbtAASBrcdf5ULU/JzTphaAfEfKH6headKaLjoXgOa46kEAwgImcGLR3ImLcAdF8eJK2LdNwOLb/T7GsOaYTj4z4GN3Sw2qd9uLfbi3ttLZMTfibmzTia1

DNWaJsdpbK6iV5tAJSaAbLSb6TYyb7wQqbSAKybVrOybQbJybHYaKa2QHybZTUKa/wCKatTVYzJTWpDpTS5anLdqbAIOZbXDkqaVTY9ds/K2yNTd5atTTqbTwRE1/QAabbWULZTTeNxzTcAdLTdabbTaBatAI6alGi6aq/CcCKbB6avTT6a75Uo1/TdQBAzVzZ18VjUwzRgYIzVGaYzfGbEzSmafRBmaszfV4czXmafRAWaizSWayzRXIKzSpCqz

epaYWfcTF8dHBGzbDZmzWd5+vDQtezahDnYCEcZrU7B+zVnVBzSpVpLXYznkWhApzTOa7TVVwFzUuawFiub9MmuadrVuajzS3Z9zUfdDzfeDjzZUczzVN4tAARbbqURa7zWRbnza+aPzV+bfzf+aRsVRBBIcBaZ5OlbNAOBbILdBaQCLBb4LT6JELQ/IULVlbxfphaNKthaALaIEHrbz5rzQ/ViLe757zTbdHzc+bfbFRaRsTRaagHRbT/AJb25E

Jb4ziJbWLYjstABJaeoA5clLbvteLQgB+LbDaybVS1mLUjsqbexbKWlJbX6jJbhUXJaBEQpbJrgzafAn9AwVfuFuDPJqFkmUbJNV/5VNdUb1NpxZ6jdprHwrprChcHh9drpbNLdBBdLUSagPCMSSkGSaDLdSa6TQyamTbqaWTWVabLcTZXLbybcvD5bhTXZa3LbCyJTSFaY2V5aHbRFa/LZbbFTYBBlTbgDgrWqbQrZ+xNTXKafbVFa9TTFbAIIa

aQQcaaG7AlakrfGcUrfYKbTTtaHTW3YGLa6bxAvrlPTQXBvTRnRfTcVauIAGarLUGbxbZLiqrTVaIFnVaEzTpRkzWmbMzdVxszUfdczfmbCzRgZizaWbyzZWa+zUHA9bbWbCLQ2b6Xk2by/pNb+gG2b+gAtaA4F2b5rbDYLictaa8UJ81raOaakVtbZzbtbFzcuakOsdbNzdub+TswA9zUxQDzfpkzrfjZNwXdb8LajbCLRjbVkFjaCzjjaKLe9b

PzahcfzX+apqkjagLSBaquL9xgbVBbLwTBac6HBb3RAha27OAdkLX+BSbRhbhLQjafrUjacrSjbhrbfb+gPfbtso/amKpRamGtRbaLWOySbazbybeLtKbTZVubYh8uLYvatfEzaWbc6bBLezbhLSxbiHeJaebdlVpLVP9ZLRnR5LaQ7FLeQ7JyajlBjSvZhjRBFRjaTKrDBMaHNpjNbdfbr+otapnda7r3dZ7qiQN7qi9W3y6kMW4TCCAhYQLPxe

+dobrdAkb2pg0tLRn4QUQBOVglJWMP0oWF6gvhxfxnBhhnCdKktncaAlXqqiFe8qCNSQbEdWQbvDX8rNZtErUpbQa6tfQa0MoOJz+fRMtxLHwuJsMN0UrEbhCIGQFpAIaUyu/yLnEdriICdqptTNq5tQtrMAEtqVtbyryVangv8kALETSEYFjPYIulWjMxjVALyZZUA+8NezCAMkAhAHuh/ltXyNdVlB3NfoBqsIBggDcAlHVGsB4EJqrSiDX1ux

gnMYYu8BMjROEDQoaxkpGRI8cM2sEMMxxO9eOgTcP/BVCF6xJuZA48DQ+tJlrDq3DcQbstqQa6Ba8KfDXPqKNf4aqNYEbW1DXFkgFk7clLjrvpXRMN9Z8M3aMM4YNk1tDPBDpvFfqMkjRoVjVufqGuVpJLOSghfZUBLxNd0qjLJAKyZYGEIAK/04AJIAD4FPAiQJ9LhDRoaHEAcQLFLHxX4Fch2BrAkAZSkBYZNOUugvVsm9Y8hAmMXN9IvEMVVW

qVLlTPw1nVSBH1gKt9VU7z4da47XjR46UdY6KLVejqfje9KAnZRzATdVsDlm7R0zDOl7ZQM7ydb6L4hG7E1CifrPnYVzvnfk7fnT3LYiPZwqStNsweV7JKvA3I6ciPgyFgQEkAv75KvMNxqOtgsqTTYtMANXZIGYXbk7Uo09XUNxAbQbZn7lpKi7aSArXdd47XWk92FrxQpKIABfgMAA+Jr4GDgK8UWwHP7ZAApPQADgxoAATuVhsoOBKRuruG4R

6G++13lQA1XkwuXR1EoKT1jdnTzSeYRxWu0tjVdFvk1d5i0IClrv1dhruNdprsKt4u2jd1rp/t03Ftdu0LO8Fbuddu0Ndd13ndd3rt9dRlX9dqF0DdIbvDdbdkjdNXwrdabvwA8bsTdGF2Tdqbpq+GbsooWbrk1pRtbx5RoKaHeLU1Stv/8PeJ010XD01EgBzd+ltQAWrosWjrqLdRrpwCWAFLd5rvLdhbsrdGdo782gDtddbvPdDbqIuTbtDkLb

p9dPoj9dPFADdygCDdyTzDdEbu0AUbvPdg7uHdSbpK4KbuSeg7snd07sM1nrnEeAjus2nQrfVh2lEdR7PsG+gAF5g7CgAM8DbFSxs/5mhvAEvKnecJwEg2sWsA5oOEIKqwB+AcGBtEtEXPEC0gJwMGEzIRoopdQQ0w1RQxhQNLo82jxuK12zrCV90oiVlDlR1B/PZdARvYFNGoCdCXKD5YRprAmQjhinqt31MslPisRoSN1yG7A8c0ldQ0y+dgmt

aVNSEs53qhJwdKrWGw8RS8gAFVlQADR8nhQ3gLu6C3aSBU9nEAHgans6WHa7unqntQcN4jy/qnspgMRUy3aX5/fIAB9OSIogAFPonSqme1PbPITz21uh134APz2BeqZqme6Wyme8z3aASz06upRo2e7QB2ekV4Oe3aFOekV4uegaBuekV4eeoipeetgK+egL1Bekz0he7QBheoi63u0kBRe8r0lG5vFquU8Lt4hW3JieIX6LFW07JNLoa2iQDxei

z35u5L3WekV62eiNn2e691ZetkDOe7QCuehCHueqr1Fe093eepRr1emL0VekV6heor3heoq11esr1rejdn8OuNpJLDrllOiAwptJRTIezGbMARwAJAcUCdPYSAC8uoDMAQDUx9CYDyMGyRKO3TmVtIlJRkPQrmrCeYq8lfD+kTYTrifQqLSNUTJSPpghOcNi+WOQ0766WXjoK5AhOU4IXEE5AMSKl128h40uG4JX95OZY8e47l7O+0Usup6XUGl6

VH84FVL61hzJAM/lMGt/Leix7lNEOBwZpLqaNJMoSPFEKV5KijIyu6Q21lAwjluG/Us6hlWgu1qXMqndAJgLmjd4IQACQUZVWYQhjjAbuCgEFDhsAcOYGK1vnebYmhIIcxUhITthqiWoJBa1KYa892LIqCLZAgckhKiTTRI0VcquOUCZTMbpZrTJWVD68VjsezZ0b8w8oUG3WV8enFy+Gr41fCjHWcuhqbJAbfSyiQMo9DJ1VrkCHA3ARto6eK/m

NJXZDvwDMixOseJRiieKVAS9AJAKzBZQXCCLAIkA56mUAXAcXlNATADKAHVTVYIUpNKnJ2Uqw1KIIeqUuOd52+5ImUGeoTlx6zGZp+jP1Z+nP0zwPP0F+ov0l+sv04rf/i6ctQi+bVpRNEClhEDJ7qXER+Co6OTYQ6N1VNtOznf2DARou6zl7IVNXxWEKaLlQfSSEd1QPKux35a9Z38rDj3Y+p43cerfnla07khc87mHOvw0L64T0gqgP3/C7IiA

itfUsGkEWflOA3WclBXSybVZarQzxy8uYDe8OIiGDYRWpGhE2Wiav0m4GOIC+ixKs6m1YSTdiXuRPJ25qzoDfDGYABOTOZ4RNf1ZYDf0DjLf1LO51RS6scYjqkNYK6q3X5yiADi+44CS+6X0XAWX2SAeX2K+hADK+j6Z+6yaIB6+tahWRIzZzSxScRM9XzCAJSsxbpwx8IjR9y2cYnCEyb9rVaKDrcPXMyraIvqoX2nekX0HayoBWYKoA0gIwBTw

e0KLOGACZ+poCXs4gCXoNYodGVX3zClZXLcv4Dke3FjsGuxTJTWDC+xVQrcxYJJzlPRiaCRvLt6kx0QjBxQlgb4YAxP4AJlQfUkC6kAvK0fXRSuHXPGyfVglfZ2eOqg0Aqnx3k+2qYieoI1/Gj0Xgq5JUb6wjRNEDQjtKHhWlSp1SlCe1CJ+oCo/O2LQNBfDgKGy1K9KuRUSAEYCBdC4D0ATZjWYA+AtAIv0owYUVWYOdZ4wKACJKkwOTSzQ0drN

xKLDSOwh8K4BMzDGjIGvnW19QcYoqAFyIgNKTLC/uIgIbNLjobIRoBxECcqRxhjADH3MpMgUhBq6VbOqhL4+3Z1Hi+hIUGqrWxBy1XfClDKiegP2LGnHXNa5hXGzK2VmKWGTCqAH3f+wLC385rZ7wOPjxST8bABmnWCaoQ0XOYpVTwZIAIAPdBGAIwAZwVIWEAOACqET9jp5QxR5imWIFikjbRq2V1Lzb4bkacoOCjJQ2YzHnmzgAcBVAFwDBIcE

AtAW8ZtGkvlEgGeBqGn8jIhEA04aPjnvAfsKlgEIwujQliZScoKqEYcUHYQCVuSkfTIGiI1oCZzncy1BWI+rOYMheJw4GvkNmlE0X2OhJKCcQg0xS8IOMuyINE+04Pmq7x0XBv30ZSgJ1ZSu4OaRF/00coMhKiWMJsa9xR5CTjU8GgYxTMRvVqeo1bSuwEPxOxdaf8i5z0Qe+LXswFJHwKQ06FWpDuBuv1Ku3PkKB4R37aip0SAd0NWYT0MzYRAX

vaW5CB8bcTl5NEK3AaNKRqOIyj6fGiO4FrkGOx5BbISGZ7KvTnt8uf29BGJISzR5X4KssIEGul3j6g4Nn+3j0VayJWah0n2Aq3x0U+342aAZIDwuprXrtXl30cL5Ci9JcLDDN0ZwbVYRsDXMKFB7rYLmPYTUREXT6e5V13uFLyOetkDS2RcNVGkBSN4uZIN4mW1zuuW1nhLVxVG9r0jafrQY8u8JY8yoAEhokMkhiYBkhikPOAKkM0hgoXGLJygr

hg73Gao70dCk70hhizXM8qzVwRGzUghsEMQhqENCAGENwhi4AIhigBIh7J0D+nDQLCNY0haoawHIUvJPdIJRpSbqwPdBDVmGucqzBxIa79eGCLlTtxMevgW5pW/QmeEVyt5AINYaoIM4a15Vj6og01h933n+6fWX+2fVeOpsNxBq1U/C+rUB+s2X2qm53JcsP0jMMmhMuc0OuMXVb9xCRyVS+0MIiniaae3GV+hriJ+q5nUwBizRs6xaYhyxAPpy

5AOlAeco4RzuWOMacWeMUoDujd4AkRyHBkRghCEB7CXZyhtV4Sq6ZSBwiXEqRXUkSkoqkATAD48tYr6hpIpMSodV1y9gNkkVf2oOaMysqF+B2CHlQTc1FRAYHMKTDW9XnCSQPRcL3obRJ9WhFYSXSK2yaYzDgDVYaTlClaYClNfDyIulVDHKrYXNJEoSuSr8YY0Rf3QJb4NE6tF3BqOMhEpAcYAgIECXGnxhFhFj1nS2JQj6orXOOhl07Otx2E+4

8XE+7300K330cu3UMNTOeI8C1QYqiMBz7gFn2xGpISr+pFWwm6QWDasAOYhrASNtQMM5G9E1OUbADaTaaArhtABwALQBEAOiDAwahCgQDgDemyb0O2dXaZ2k6PmAYsGdwYIBA+fPyWNcFrXR0hFsgduTQQaXzBgY445ePaMRoOrgWQLJCcAT6O5kh2xoQY6OaAU6PIAWGwAAPxXD1AUtep9VG2wT2+BjD268bJpOsqtndsvtlPqbVWZgmMbUhnXg

b2keJBp3XiUJGlVPqJEEnsxMf2xpMfJjOtxJjiCM681Md9sqMZlaeMEzgbnQZjWuM22nXkNRK9JpjQQN80VNgxjjMbZjzMf5jgH3Zjy51FjyuN7gcAEljAsYB2ZMYQAbFTVjFkCpjCsYJjMrXTo6WT5jEL21jEUG68MsZNjWMYEuoh0Vj+0HTo9oFVjVseZjVsY5jsNlPqxcDpjjsaZjmseOsssbUhqAE68kgAtjU3lNjlkHljNsf1j/NQ9jZjX5

j/f1IAxPzlBPsZZju2OpsCcaDjScdjjqcaRqShMUB52UkAShLg6GlUA+ewxjjlse9jWsb9jbMfTjt0Mjj+0EyQXseljmsdtj8l3WezAF7EAj0rjMbI1jNcdhsMmXSOfcF+a0MdYgs+z6A8MfdBX0bJZw9X7hyAHytBcGjON7oi933la8nNlJAjkAgg0tiBjpVUOjwNgejZ0dBjTEHBjK4fbkd0eJsMMdOjT0dhjtfgsahrQ+jR8d+jsNn+jdJCSg

+0ZBjF0cPjN0fbk0Md3jY8eJsSMZujKMYwoaMYECDccpjSNSstuMfLtbdkJjpcZDjcsedj6wN1jEcbdjMrU9jncafp3cd9jZcbZjrsbbsXMf5qPMe46aCejgQseXpEbygTYsaNjICfQTwcdZjXcZwTCWTFjDsaITgsfgTs0MQTBcGbjhsYljzCcTg5sYzjWCdAT9CZ72BseVjVCe2gfCa1jLsb1jyCajjqCYET1Cf4TsCf9jgcZoTUsboT0ifITs

iZgT3wLjjCcdUTOLOFAacf0Tuiezjy51zjyuVOs+ceXOhcY8tiCJLj9MfkTxCf0TcserjzcfrjPCcSg3cebjy3zbjmsbETLCabjvtj7jZIFDaQ8YHqlSB/jfJInjgdndtM8Y4AedpmBz3gXjO3vwAS8eZgK8eFeKkHXjM7qa9MXRa9C7ra9fJkPDGms02DRqMW/4V2jL8e3jZ8cej50c/Ql0Yhje0OPjEJKogNSbogLZ2ejV8ZBa70cGqjSe6eP0

Y4Af0dYgT8c3j00DqTYMaujR8a/jsMfMAkSaogf8eiT4zSbh+0HRjHibNjYCa8yECfKttce1g/ifVjrCcETGiYYT/NTkTSicbjFcccTgsaETnLWWTO7t5jeyc8TwsbITxyf2gDcHFjxsbOTXcecTpMeuTtybeTTCcuT+yaTjUiaQTmibtj7yYeT6ye+TbCetjHCZ2T9sZVjayeUAEiYpjfdPYTsDx2T0cYcTnyYUTFyZxT0cADj1ccwT+KauTRye

ETWiexTOiazjxiazjRKeTjBifjjpidEO5iaQqTKYLgNidiT0cHsTkKbDj0Ka7jriZ2T7icBTniYb23idbj7cZ5TAcdFTQSZ6y/cdCT7eHCTo8af2iyanjxv1njiScL8ySb9N0lXSTqFrXjL4Zg9b4dM1QjoQ9UQSQ9vQr/VpIVcj7kd/cMYZw0whG9W8METU8aRtEWMEz48NCI0KoiMYx+v5DDuG/ZZLAxk6aRY88VmAwWwdIFWPpojoQf2Dm/IY

jdYYv9lWsbD5waE9JzsSDZzpn6FwCD9uOsk9QSE4mtBXNDtiliNAahg8chWWjiItADQIaKVPgwAjkIehDaJFAj4Ecgjq2uxlLSrkjrswUjenqUjoPPnDTlFPqh0ffukqYwTocfRTNXWl84BxoWdkGXjqFqX8yScIdmDuCeuAT/AnMF4CubPMBueMKBmNWJ+6WUSgySdWQ3cB0ud3nVT0Zznu49tfAx1h3T/QGnMUMZgxgAEIrQAD+5lj9SKhJAav

SRaibOenXwHummfKrUXzTPJKKMyciQIAA5jMAAPBaUUQAD5yhXhg3YAAIlKkoqCxJ8x6ZItCcdQAcWNwTYlPC9TxAt+yJM6JkDN2hM9HPtFclHkgAENzQAC+KqG7+uLPJbAYvI57kJAmKsT8obRCyVsURcqM2mhifqJA4M2hmTATQsDuGY1BfK7t6ILOAsoKgAAAKTMAOQkN7XQA0HY1o7NFbFMVM3EtAJyQNwdbGZgjFn5ABiB8ZhQlj7eUGfI3

zIN0r2mDI5XzBgXMCMZkSSaAKoALiLiC421aq6ppRodyISDhtGdOl+Ih0P1Gha02ia6mQfNli2tKmEQKTNpoDuQnwPGEEXQkOegGUB94AB7dwBuDVYPvAygLKACQYaL0QBTOKZmsnoPZTO8Z/jPyEjTOygrTMCI45nuBfTMJNJiqXoYzOmZ8zOgNSdNWZ9uQ2ZqSB2ZtgIOZ+dNt+bq75AtdPKWomxKE41neZ9uQfMqiA5ZwzP5ZkzMXwMzMUW32

xE2dlGAQGUAw4vVF3w09jYATOCIQPQBsZnLxJZ1TPiXNLPT7VAD+gLLMn+ErOkgazNUtSrO8MarNOZ46q149JoFG6Bi9pm6PEBAdMHJtFOwp2B73xsB1t3cdN/gDbP4AadPbe2dN42phoPZxdOXgZdOJnerOTs9dO8NTdOsAR6Bvp+9D7pzjKHp4bZwZzG1YZ59Pu+S9Ofxm9P3pjAyPp6HOrIV9Pbe3dN4gTjMC+WeS/pknwAZ4DNgZyDPQZ2DO

Y5/oAIZpDNWA1jO7QdLEYZt+aw5n47HmgjPEZ0jMzycjMLySjNZ1GjN3ZwbHSZv8B9ZhjPc5ljOoZ2nOZZDjNfpifbJZwTPCZmgCJx5ird0VUASZqepRvPrP85v8CyZjMpxZ3SnPU+bMpZxbOax7v4ZZtN5rZ6/ydZrOrdZwrP9ZizOcADJNbZ2zOvZ+zP0OxzMkW5zN/ZsIA0tE/yeZtXOtZ3zMaPAB4LOILMhZsLMRZqLMxZ7XM6503565tTNl

7JbNV7Y3P1vU3Nm5wIC5ZtNCW53rNFZ+FqWZzbNlZ7bOO5qrPO5mrMw+OrN5shrOTkprOJ3Dn5MVDuTtZ20gp5rrMFZjPPW5tuyDZzbEjZufyw48bOaMKbPjbWbN+gFTP8Zg3ON7T5ErZpPODNbPPPZ3PMO5uHOEOwvP7Zq/yHZqNoZNKW2zupTVCGFTW8mRLode7vFdewxYgBCpNeyU7MTx87NIpqVPAphBPXZkdNQ2h7M6p23NTp3nw7ZqsDUB

byqfZjNg/Z7PGTXNzMP+BypA57dPk599Pg582yQ557xo5lB2M5+13w5/VBXpwSF3ph9Mk+J9PecE9OXzYWyg5j9M45zbh45v9NAZ0DPgZqDMwZ0AuvgSnNrM6nOi59DMMnTDN2unDOVyFnMkZvrhkZ1C4UZ3aGGZnnPE2cA50ZwXPecZgsi559OzZovM6tSXM8Z1TNCZkTOaxsTNK517aSZn3P4XDXNyZiPPTxnZmLWAfMx5l3Zx5yfYJ5nlE6Zq

jHJ5gzMW5xvOT2TPOWtCfP25irP553bNz513PC21zNl59zNE2b3PjVHzPLIPzOQMgLNB5v+4h5yLPRZloCxZqulaohQvR51LOG5zTMEs1enUM7LP153Qs9Z/QvN5rPN350rPlZ02CP5zgB7ZiwuZ7d3P8s8XGao5rNV51rO1583N5ZvQtSFjSqt5rckQAdvNiATvNrPbvN9AXvNi5/vPS5ofPd/UfOaFpsnrZowtT5kwsz5p3Oc2myrz5omyL53X

xQe73ILdMCJLdHbQmpu0xfh1NpiOqoPoAZIA1AGMB94PdC4AarCbMcUCegPdAcAPGAHwRiDkgGoA8qpY2fs+1PVLc3RnEfEoZTbY28AOqNoIVbk4ZGBWGOvYonAX/pb8SBIERilJNucjigIA8CS6NYRtRp5XYaxx2ce7qMqh3qNMu01UahgT0pS7UOjR+JWsOC4CP+4P0tavKUb6lzlfINMzdagsDPO1zhHLYsAUDccPp8n/rmKk1Iomhv1zh4MP

7RewZ4wCgB7oKeBVAU2LdwAcCbMSQANwGAD0AKzA7wdzI+SX+WGK3Tn4cKUr7AZoj4lRISjBlfD3HYIQBxMNhrBuv3JSN0YoSq9Z7IO9Qdpt4vbaI8DKLJHBI6TLXxSMNMdRl30hKoqaHBvqPHBqdqJp+fWsCu/2U+/2gGqIJ13OzNJJCH7X2yxUtye5rZRxNBBBSPEueyskoMRVxjQBrtNklrdIzF+8JEgHyiUvegBVGvKPFtTFjLczFTKyUXoW

83X07AI5TfaAYwe0KUNRTIGSTmatzJasOyrldcpalncqRpvYOu+0JW1hgn2Gl79asurUPJpmQapphTQ1xIaWTRvoa0ctNIk6nHDdTHUJoAcNg3IQEB8aqV0Bq2SN06uYxEaRkI4h7CoSAOa7pyQADkmvcY2NoABsuTtdP7o4A+BgIzYbrMFdvka8drubdPFCQ6tgMgs85f0ygAGmvMgyAAKKN1y0+6eKIAAABJnka3i1TSjTTkqltcuE5anLo8ln

Lu0PnLi5fwzy5akoq5ZPL7roMy25d3LB5ePLu0I3LF5avL23v98t5ZyT8yUxy24aiF2i03zNRs690hlVtPXsfDXsjHLacknLdxhnLc5Z7db5Y/LX5aArp5a3LqFx3LPbv3LR5e/L55cvL15dJAEFcGL5m1p5oxd3ZoksQ934cmNmMz7weMHcksbgyCa3Epe3JBlAIwBgAuEFnACQG5IdqoOLf8q/ZcCEhgW4kAQWYcB9AQncctbTdWYSAikSBufg

wMUI4aEoLSEIzjDkDgEI3kpwjuZeCDXUYNVPUf1LoJfoFg0ev9Pvtq1rYf99sJf1DVzvuDeOseDl/I6C70Q0GRkRyDoMrBgVimJYxvtLTMkdADWntP43vE+Q2fNRNjft21lQZs1B8BRgVQBwAB8GZLdqfu1bjBRkAwwScbjGFLAQlhkwWtiI9xSRAEWyLAT4nfGcmySEscUpSzyGcUkweRoH6UNYYyx1Vzyuojuwbw10abd97xo999Yf495ZbYjU

JbNLbYYuAnYYNDwskY1MqGPaG0xEjgrpFd0CEjUMcVk95QFPaKRvhNYVb9DVikcYw5c3mEgEAAIJpVcVihZyY+RSUfSj0VLiiUUVirS2PasHVzORHVk6tPmM6sXVxRbE0AGX6RLYTZSYHW5JyIXx2Vr3wV5d2E5IUzJC/fNXJL2RXVliiHVguTHVvSinVviqPV+ittCo1MjG+D0TF1itTFy73+l2cBNABMAEUokB94cECZBG34tAG+xNAG1SSAFE

bKSQ4vilKxTlBaDA26I5b4unmWCEVMgAIMCaSy2jRAgfFbQiGCU+sKWXROWsCLOwKtyEA8S4KssPNVhjDO+pUNhB0/2xpkst6ymyusRpNOUaqsv3+2EvdB3iNJc+n28C5jyZaiATgm6Pltl54NGEILxultI3kbK9otrLav36kMNTGrKBTwASDVYZQDDVz73BpYmiqEYPjnIdMuvFwH3Ti9sZXuAQg7KSHrZhjERHACET9ByGbO4JgZYyFYCg4Lti

A6npRrB00pNV+UPbBiNNtV1w2FlvUvFlo4Oy18Et9VhWvHOpWvmlzQAXACSujVmiZpBgSNI0U3QiRgF2OljVJ30DKRGMY2trRmkWBkckJFS2cNBh2PXC+0MPgu7kgJgN70DgLKAiSJ2v3amtrpTO9Tp8B4J+8NKzGOiKSkefnWLcuzkI0YGRhGM0YClgOtihiuDfAGKRR1sJTeqUuZal8WtVhuiMxprquMRt43MRj42/rI523+lNPK1i0tVG+jUh

+un35SkMzRa+2b2ytELDhv1ThpZXlLV5I23LUKtBq1uvQq2eadp8AW+ljGb+lwpavyocQHwJEoYDHD1IuwqNr9FcWvIAhARmaeuJAF5DujLVCQ4BGiJG+f2PIEKZGMTDDTi2ziYNpUvzOksNaWbVVJ1sWsbOiWsdVosvS1rOue+oP251k0vfGwauOVi0tVKJ/0Map4MM0fIZ0ZQcNfIODaQbXPrj+qSMgB1avANvQqgN1XR362bZwQ+Vy9QNHJSy

tcNbhtfPy236to8ld2753vHq21CvJ0VRt8O18OLdY71mptisbdJGunOZcL2DC3gCQHXjLa+fr+a8yUeWKPjNEHyKzAChAM6yxWTmXxxEeFER9xMKwEu7HAVtfBA1IC3lnBZ7lwcpxhXLbOZ2cNfrC1vf0d9A/1PrJhvp1vH2Z1g0vZ1941nBrhsjRnhtjR2EuMG1INr6jWu9hFpQyitabDqP+u8KmOI/OE5BN1sKu9bTZCnBC2vmGbuv2DBS5w3a

rCcQS53KSekMgJKMyFCOlLeKrFgJ8yxUq6VtpMuF3CZqo40j6CMjrK08iwya4CvROkLBmABUh8M2swqiiOsehjC7cpx3mV4EuWVtUMDRnOsk+vOu31gutDVkI1dh5/3MGmjkdrFmvolvnS+V7g2lULVJ+OVpvyN5pRR8pcwklzuupRvEP+l0JSBZ8Ggq+hF3hlgIR4IOPirAV2sBKUAQzNlMg4l9BzWMHyJs1l1Qh8JlYOFAjQYG3QjrlTEK30SJ

wiEOYBal45uAl05tS1s+txppiMJpiEtsuxWubLXhtF19AahGoE0ugSGaQbfwOf1vluzVnHAssYnB/B5auANuRv9l7pJ6FIgQmckFuSuHEwjYkk299cgCFGjcyKtw20YTONDcGUHAURfNyfDM5Vf+rRvQVnRtwVtZL6N/6taa7r3abXr3oAKFkattwjaWIzWGpyxvvh6xuo161gyKi0xLGewbaJA+AJAObD2gcUD0QNgC4QKAD0QCPJyMCYBuDDp0

6clY0HASHQhmSjgqqi4vnqXlTGJIhAICTPi0aFZvxmBIwyETZs2GmgYIwWDAAIPTlT10sNpN7gakJJw06l3H2HckEsXNk4MFN40s3100t31wusXAYwNq1krmv+n0Wk62WSBGPWt38pCOCt0IYe8MyN/NyVugVAFudNjuvbR0FtxVy1ObMUmZHMYuAJgXKPqG8MuhDKLaMSF4DfAEhCWh0zm2Sulh4iWIaPFDpROB5AR7FeqUQYXIS9gFpQ6i8OKI

JJLXqee9u7+uUP7+6tuKh4+vKh2luX17qvxphsNMtissstt6WlNi0uDciT1ct+yBlue+Dai4YbsRLmJE4DQY+Kzn0CaoBtTt9psyt3STKNyTWVAXS3S2AjvQ8nVvIgRNQYYA1sU4I1uKa2LoFJvRvb55W1IVq1vlJ4GvJ0Ijtw1oY0I1wR2TFi70etlivgN+wYowZQAJgTQDCQTcBDNzduRzbBvesUtu/oXJVHtmxQ9Orss8hnZCZhUHDOlktXrc

mw3QTP9J0Nz9uY+vBwnN+l1nN3JtWVqINy1mINFN+ysJB++uaAE4D1lvbCSJMWbvNiyWYlzaRrB6ZhaoSdsX6luvStwFtKCiAwpeVf61egPzIAILsReka3nZVC0lUU7Ie2qy3hA8q2w2NlpKNEqh94HQHJdvtCC+AgGpdggGC+UuypdsezUAR+S4ge0Cpduyp/R1FmIVFXKUE0AE+QUvaq2UkAlUP7LsEri7tswvw1/c7ypJgxMdduyBd0daxEgG

zJ9AdruWgfACXVVaz0GQCDwxyfbIGUuzO2F0TIABqrd5wIBDdoID9dvMAlIKwCPgPuFUQJg6CZQNCUJ7rEFcdayC8EHj9/Drs6U+OptAhCwjY4LuwBfarBdiLsWJ+ruygPtAxdp+lsm+Lvu2RLtRd7jJpdhrsZd0q0OnUbvo1ay2SSNLvu2QrtQHEru2VNuBSVUPYPd1lPVd4Pa1dtenpd0GBNd9PYtdhBnDbZbudd07vDdnrtzd1buVIHHtA906

zIGCbtV7abtc2Wbv5iebvNQSbNLd1zwrdmmzrdicBbdmUm7d46MSxg7tIBI7vA8Dbh49oIDndo7MN4g8JSyKjvNetvG0ds1v0dgxuMdvfN94kxtqt3C03d0LvXd8LuVd06zfd0GCvd6ODvd9U2fdtuxJdv7vK5X7vPd0GCZdwHs5dkHv5d8q0Q94rv94aHu5IWHsVd59rnZRHtNdurva9iR5p7RAyY98UnRnHHui7JnuewHv7YAPrsDd4kAh90nv

jdlACU9vGM7ZXrt09k0EiAKPtoRZntrdnIAbdiFo0M3hic9/btAsw7vIAY7sC9kPvC9pfOe5OJYcdl1vGp7jt72dXSetvSzetzGZsAF+UIAIwCXoGeC+NzQAKSaSTJBYGiQdrktq+4NJqd1MLnBXli7ELZUGIEgadgCjRBWeGJLN7xBAydYCtLKZj8OGI2ER7ETFzDZWjGZxho6XTvpN6l2MNn9uS1+iN0tmWvsNyqy2V4aNWd61VthnsBWlgSN8

sf+zeK4dSfjXhXkYZpKThLzvFBnzszt2Vv1+kp2vqv0tP6pku4AGUCYAbsBpV52L5qzTy9Kav2BTCV2A+v4ZPiJBDGEUKQmpYNShJS8T8rNYPBpmw1suA5vtRvMup1nH0VDetvnNrw1gl5tvAd/quVl1lvgd2zsojJ+v0uTPCPFSnUb9oQVlS0QVIIR4uqej53qex0MYd7ztrSLDt+dtE2Gepyh1mlJP62+7svtLXuo9sN5Sm8BMfd7mxzJnC4O9

0rsw99k0m9q2xm9kqiW96Hak93Ltc2W3vu2RM5w9zXu1dqgme9lHu6DvvDo9v3vos+7xVw+mwh94Pvp90PtJ9onuDd6PvSm8ntx9qbsJ9mnvh95PuLdtPsdd1bucQLPts95zPQE4A759iWPMm1qB89oXiaAVA5l9grIt1H0QMUQADfnjjYDvDf9eHZ1kpB4JcZB849sHikm/tpF3FB6im9eyoODe2oO3Y5D3He3ZUdB+b29B973DB6D3re3l2we9

zYLB672zso92Pe8j2nuyl3HB9WTtcc94g+1138e3+BvB5H2Se/4PXdpN36DDN2k+wt2GexEPhu1EPWe5t24h9DYdu33g9u0kPLbSkPi+/z3heIL3xzVkOJmjkP8h4UO4kcUPJbddRxe3DzFNrBWfqzL3ik6WhNNaeGga30avZNIOiAhUPXqVUP5B+MOXu6HGNkztkYR2PZ1B0V2y0FoPne+0OUu/oP/u1l3jBzb3+h1xdLB272Rh0yCxh972HB77

2ph/WjZhzcOCe7T2fBzsOggDH2Ah2sOqe5+wQh+tYth6n2ce3sOYhwcPmfscPTh251kh2IBUhyd3Mh39lsh3kOCh/t4ih/XitLGZt4azX3EayjWeO1KpG+2d7m+/6XZEB8AiQFUAkki6GCPOBgiwqoR/HBsbkOesLzRvAgGiNilLiBMMopq3oiNHmEEYBusegrgI2gr8Xyw+GmDO9S2jO3+2aBfS2L64y3OG623uG+237+50NOWz2HymIcRP4GCa

GtocqrQ6VRw1OW4FK//Wey3CbVo203W69h3/O37QUvBl54Wbiz7bIHApuLhalWxNj8x7X85sbnZm/qPaEIUgXJ7a+AZrQizXXHNabbo2OCx82OlrUxUVrdDVhzRr9Htob9fbOA6WeDlbcLf750LUxB4bb7YhwCJlUAJxARMlxAlrOC1HzReb5KaWdRx/g7aHRTbC8yQ6/WcTZEbT9aEHZebq/u2PsGs9aSLag6JcuRaMHe9nEdrhbCbcTbhafTbu

HT4FKHVzdebZJJ+be3TaWQzZnsULbOHSLbnx9/mWgfK4JxyeOKwEWO7W7sSazeWPZsaeOqx4b8axyUg6x9Nb57aBOKx9g0WxwWc2x+hPwJ52O00N2O/Kr2OSAP2Pnk9DYhxw1wRx/tUxx3DaYHVOPr8EdQ5x0wAFx0uPOvCuOWuild1x9Q62beG0ObW6aubYw6mSW3Z9x7hbkbUeOYJ3X9kHaenXrdEXobPja7xzg6V01w6uxzxbckL9nD6iw6Ls

Ww6fx7haOHR/nTIKLagJ+uzFFu8OIhZ8Pvq9L2f/LL2LWwCPFewfnk6HmO0J7BPcJ0HBIJwSayx45O6/vBPWXuNax7SRb6xxwyqIOJPmx7PbWx6hPB2GBOmAEWODJ92aex6/U+x0tiobRA6cLVRPWbdA6KbRpVpxwxP6J6jtFx4NVlx9NxVxxxOUp1xOCHV0W+Jww6abRpV/6cWP9qslP0sqJPpsRFPUdpJPSLdjarx2mg509g6ibbg7Hx2Q7lJx

Q7VJ2+PmHXzbWHQLa6Wb+O03pYWJIP1OeHbbADUz7l9LLX2lR/X3lyDIqFp+qObNXyRCAIsBB65oAExth79R6rzi3A/RkQBWNhVJsGzR6VXZxZlJ+S/T1wm5MwkUoilYQOhhMzDYaO1jca8tYf39O2hz8y+1XsmxQOTO422jS7QObm2227m2y2JgGBqoOxGPgdJDpq3OaH3+7kHgpRREthT/2MQ3/2OmwAOA5TmOnKANazoKWPEIahDu5MPbnkYh

OcgMhOSLdPbYqCFOsJ2FPFrTsdAJzFPCJ3zb1rfbTVU0mc27klOoHROPaJ7DZhJ7VPDx49aAaejaXrW1ODCyLYsHQTaFJ1Q60LTQ6eJ3Q7ui2JbKp/+P9J4zOVIK+PQGqTbSpwXnFZ9TbNAG7n1JyNPNJ2NO3BvpBdJzbiAJzNP3My8O4LKq3ww33alYPjP7W8QE0bSNbNtmNbOTb5PWzShO27J2ajIHPbfZw7PopyEcAmh+PWZ+Ob2Z+ROZuJA7

UpzzP0p77Z+Z/VPBZ9fanrS1OLx15l2pz3GVc7eP9qveOep8LYtZ5uPZ87rOdxxbPVZ1bOH/BrP6LRuP5Z1uOS5wJOhp8xUNJ6iytJ+1BzZ+OzLZ/hPI2jbPYeeDxL6CZOFNZL353RvmfhynZ/h4DXbJyx3KgHjO2IATOXZ0Pa1IR7PbbV7OprZTO6ZzPb/Z6FPA57Na8JzAACJzq0V7TbSakYlPhx093xxy2a3s3zO4HQeOH5ELOdyTebRZw/bM

551OpZ91OuLoXPa58XPyp0rP9Z1NPop+rPBp5rOa51JBeJ2TZ+J8rPG56HOmnqNOvx/Aw253+O9J9NOu58pae5+2oq+4d6FR1x3lpyzzVp3x3AXazz/S/MBbeJgATxr9geg8srNDc44jdOjhkENnNw65YrCcPVGchNeJRIxnNAUNGYCG3ilLVm9P6grv0hg8E3wnCZXWq2ZXvR2f3/2+fXmXVc2hozVrYlQ5XGBxMB9p6XXn661q7nX4GHtUO3iY

Lvl4x76K+HIwNuy4IPey8IPf+6IPMx+IPwG9mPcQ4u3RfZUAcoHtOsoAfAOANMAR67LzYJXDFmNTKLfGwwuUHMOK3upkJxG2hr3eA4VeNQTgFm/X0HxBkIE+NrX/0NCIhFwCXj/Vx6xF76OL+z1Wvfdf2ZF0CrrO4XXplY/3UxsUQ3GO0FPg/HzPm/rXY5RqLkw2h2z9X2WRB20rfO7O3zFxIOm/T022ecIBJAIHNBQNAPLus45TkBOFfgOGkd+l

LLhyPekFUFFgKPUZWopo+JJZDUkkm421egu5cFmyEIsQjyHGqwf2q27qrIpb9O067qWcm6w28m5f38JhZ2gx8U2QxxDOmpjy7xq+iVfVKyppG46WIrHiVmgtlq0Zzz6vZf/2cOxJql1hIA35jz2KwNOahuLXJrvJRQmx9g1pbJ8vC+9g0fl38vQ5ACump2EKn22EY2Bhm3+wqvmaOyPPLJ78PEK8a5kK9a2lex8uvl0wBwV+Vx/l4CuKwPNPhi4k

tXW94UehSqO8F8U6MljZr5kDPBFgNyLaYuBr4Ug/BeBwGQNCLd0U2+/A4gLGEIeseJkyBD7ARn/BYZE0QpxUk2qq/4o5lwMwwzBCs9CrEv1l6QOT/YkuDxckvAO71Xrm5Z3ZF5kv7+1h6lF6wOCwPEI1+vwP3g0oRf/a5wI0qR5ZmI8uqVaYu6l6SWdo17JHjOZbNwWt5g3cPI+jtLZnV77bSAK6v3V56u5NbCvTCBW1lRD6mJQB8PZbV8OLJ3jk

0Vzvn5e0Y313Ta2IAN6vI7RWA/Vx6vejiSuLNotPFR263lRw33qV4APaV5ammgLgB6IJn6D4N3BGZXqP8o/RxkZMuVYhuAJeYmKqJuReI0EBRFUaCF57pzCLA+EqVAMP/0mOVQ2t688gdOyLX6G9qWsm1suAZzsvTO+qGaB4GOb/WDOGBzCX/aBMBOS483BG5fzaRsip4ZOCa6/bwqPQs9ylgCe0AGxp6jF+jOTFzywuVBW4Gl8oKvZPZDi0T01Y

bPkAWMqtm27PkAgzm+vibB+uSAF+vobPkBDWX+uqIPkACAUBu/QKXZ7rJd3k6A+uCPuoOX19F4wNx+vMIIhuNfohvAN8+vQN8+uINxJtDWBL28k1L2UVzGux56UnMV8x2gR9BuwGTGA4N6+vn15+vaN7+vn1+hv315hv319hvzG862Ri1Y2KV+amqV3tqHG5jMBIA3BjgNSGSAOuua1+GWo+SjJotaCaAUFgIwFTjhl62W5IEjUg1psLKqqFHLvk

AAGTUiaHJV8ww8EG6PRaxOuT+8w2M6zOugZ2WXNV4cvb+5xH/HQ1MJgHRqBG1Vtzl6fgFJqkrwTQ02kZ4Roo68YwbV4alqVdevDWNjPhkgGJekcrcAbtRuEN8+uPYChvkN1hvLYYhvLEZBuSh17JxKRFugoIhvot7RvYt6xv4t8+vEtzhukV/knCN6jyrJ+POyk4CPxTAGlhzSrc0t+SAMt18cYt0wBENw+8Etzo8EAElvK+9Tzq+5xvyV8rweNw

Wu+N2AL7BsQBWouRKagL6kOl5oa6UqHZorDdOEnJYruIu2MiQmKuKPRFsJCKcECZciI00iGmxi7mWdgyIvqw6fXxF36PJF/OvLN4uvgx+DP5F41r9VwBpr9OOpWQ82WB9doud8mxEIhH/X/g1z6ql8Yual+2w5CIFvcO+8v0ADdCOIDABat7dYotw1vpbCDvcquDv6t3tAOt6L3uDLhuI1zBXzJ8Vu4hbGuGOxiumOxVuwAsDvjCQDSwd1FvIt++

vot1mvGK1xu+tzY2j2XY2TLBtPLU/gAWgFC6RgMoBHQJNuHEB8AIYM3l4HABhUS8KWjii8UtxNeJihCEo3FJZL6aI0Q0yDbRGPUOvgTYZZE63p3k656P4l0CWfR6qu2GykuOG+du7K9qu7+xDPH645uDV5+UtUEaUZqzcvew653wsIjo1hAUHgq8hsJW9UupW39vaehYuRy+gAhSaGcCvCTv0t5DuEd1lumt3FuwbrO9EN/hT9AK1uiYYjveyLxs

JAJ7uvIXDu/d1KBGtxDuct8HuV6aHu7LQRSI9+Gio9+Gu+52GIUd6ZPI1+jvv6HR2sd3L2cdwr3jG3ZPKgHHuRSQnuydw1uA9ynvv108mM93DdsKdnuiIF9Bc9463oPetPt2bmvuNzTveO4Nv+O5jMEgA05bJLbFnF5d1Ft+RoKPaFZjyIe3kB8mZA0+jgTGIyxaNMcqEEBEIbxO8hw1HSEvhpppyO2DFhxQZvx1yQODtyfXOq8du1Vwy2gOwuvd

dxkv9d/IuV9RU3Q/bkvwjdAllCiJGo/UjOKWBI4nRr5uxFVev/t102elc37/S4QB3Bm6kD0LOAp4CjAG4FZh8APQBNALNqZ4MgNxO35IpK6YpCOD06A2AnwV+/SL1hYRpJRe45kQAExMI8gId9+at41AfumdXLv6iGgHX9OMBxgGmYZQ4ruvp8rufp0quEl0dukl5rv1V6kv5a1quX9zZv2hjP0EWDkvWFfZAoEgeI9QpT0NF4Z5GaB8B5UNTqvt

+eunlyWLxFQFuIDyC7FAz3XXTFABFgJswUYNsx6IMXBDXLqpNmGWsWgHG5PQAOBA+UP3TA1NvoBNeI4MM5y34PJu+9J4p3aJ1Z9VkFMK+qb7+VOxFL1aCa5nRXBnonIbWlCDMPQmqIuD6sueD51GKBWruVV3dL79/6PH9zrub+3ruJD+RM7N+U3u2+XWv99AhBFVilmyx91zltilJwpwOBBw6HDF47uft87vXVDev6lzFXFDVYvlA+hs72cZm6Jf

gAFin/y2AAOBHQtyLCANyRnDZpyKaxGERg4cBE0pNzNNCSsoQM0QHFJnN9lI4w7izQfpMHQf9906NnR1jJl6/XWBhq7WRgxfuldx6PeD9fvf2+keytSdvqB5fXCm1Zu8j1cGkg7Z2Hm7du3Kz9Kn+yUQYYqQIEOxxrR28jOLlGGvPt+h2mjxevft60fAt9kagt3TvmRdYuJAPQBu4Lf0ZQHUAbfrPvNDecVgBKFqlpC44BW8gOgpAWryJB2x3aPg

K3gEHxy8qEocwjKGkpjQNUaIFNPhiBz9+2Ovzj5SAj64Z3Dt7fvBD7sutd1f3RD08fxDy8e002hkJgBy2N18oukS0/3AkvENmfQ1tOYu+LXRq0ouyyAe/xWAfXd3O3YTx+HyS5jM4ALMhJoAmAhAAg2YW46oGOPowcRMEI23Bz6j22qrT1AjR4jfhxGD76mXQM8hEpB2s6RpUFCW3lIXVOA4uVHv3fhofXj+xyeb9yw3z+0IeH9xqvpFzQb4g6/u

V17Z2u2+KfjdwxM1D6jRhmIjO/KwYgg+OgO7SzI2AQ1ofbV2qe2jw6vJB17IucptkmKtLZSz+NU0cueJvG9nNBa3aeKW59WzJ8ptS96PPRtCRvcd5PPyN5UBKz1nUKd7B76edgufw44JVR42IGdwif0ACMADXN3AVJb2IMT+5Y3RtPdfYnoVUXblX+9bhwM+JlIkhGpur6KVWtUn3FiWA+2bDR4V44v2ZrOIefmT5W371ivyVdxsuyB3uLStV8rZ

15c2zt5GeyfRxGhTzWWpD4P2Ezw8Gvj6Ue+wiaNMVKh2uB2Ypil6OZ/Kw3XlhCevUxytGbPG02y3GhKQOfofRz0YejoosBK18nrpgAmAeI5JXuS7hE9BDBMIjERoGImufOJnyXH6Gjgg4ov3DNOhrjz+sBkwuo6VxaOurzxzR2T16POTyGe792GesjxGe0l1GePz9RrXj6/AZD0I2aa6DE5OxbuccCVL0z1VQgpO/AxxfbvOtnme/N4hftxA6Wi1

7fq3l6hf7BqClMADABhIFUBsxRzvu9Azri5qwNxhlGpb0lipArKEpwYqQglgDRenkLmkUQGTQud24qKBioR4JdOKfL6RkK2x+3uDww3D/bW3yB48LAZ1QPrK1Iv+L++fLg0JfhTw1NrkPZ3+1GhKSiPuvt8i2WuYszQEYNIkVT8Jq1L7JvXl8C7Au6CviV1Bvle+n9XXDhua2L5earx/BCtwRvWz6iviN6u61bYmvsV7a2SryjzTNuguLGz1ulp3

muVp9FxUL/xv/SwQAEwIN4Z4BHl5z99EIyLGREW32MX4LAlrkC8UV+9WwjllWNA67oMvkGcgBSy7LwJm4q5+FMAfL4hHkaCuK9tynWrj6f2BDxrueT8Iftd2+fmw9Gf8jxwL1EswOjd3+e9lkI3w2CnMjir05lDzqIOggQ3lCrlevZemlozN6WIG13XDD/YNcIFUAZ4NorzgKor9ANVzi4M4BL0FZh9AGhaEAGGO8L8P3xSpFs5r20sXgItfC+rZ

we9c0E/VDsgDwEBM6L5v2GL+LIFL8xezj4Fe2T4GeOL8GfTN6Gfbr+GeRDwcuLt0curt7GfxgKJePK7gVAENTeGtjlJ3xTYxXRkJN6j9JGHd+mP5G+ml/2She0o/6XhIOWJpgFAAJgFlBoWwdPa1wilccO8gBFSv3QtXUeyo5GEsyLhxEW9CrriCavkpK9FSBhNyLAyuU3p/mrXa7VfpxbY6Ar0kegr5k3jN/9Owr2ZuIr2Z2or/yf+b9ZvPz5u5

LgElezxK3E4OyJGydbXWsSz0kdpKK3T10IPwT9oftwNSqGJELq5W3evTG961Sr8lvi7+E0gV9Dy4w3Pyvb3Vemz8XuWzyIYy981fDG2u7HBBu7pXCXeur7KOerxxuyV/1fh9+63eNwfKht5jM6gBoqEwPgBNmGwAxT+JunHHAIfVLtebQycegtstziwMrJ7igirxd9vW2Br+gkrG4q/A1nMMpG24mo1DqiB38WjN0Gfrj9deMjzxfTtw8eW25Hfn

j3Fevz2hlDHHHfokCnM4+GlfVpDKGD16/BiUrUsQbyWL8r8hfb1wF3x6DGA9o0IAUoMNmSr61AU8j4BtAJIBAINBBKQFA/fAClBabE9lXThr9cHyQBkH73gBk+g/EkJg/EINg+Ywfg/iAGJSGsv1UcGEwBNwJP5JAEQ+0Hxg+YH+Q+cH0IA8H1w+SALjXKfsw+4AMQ+2H7A+IAHba2QMg/UHxwASH9A+RHwzGJHxABWH6Q/2H67sVw/I/bYESuCv

MgANH2SzEuzDtSPi9VHMuQDUH0Q1HMqG8/8QY/l4wQccgFxBjgMf51IE1BtH312mpzynsY1ZakU914Zu+4+oqTdmje3o+kctb23bJPYCu5dY+UHTTBfJY+ZwJPZbHyZAK7a8smp+tYEY9o/ivLE/HH1o/nH/zHtH915ZlcdtPbbMrtAH4+Ap9JVS7HZA8n4E/fbCE/oqX+BSn9ACHcekmrH6dZKKIAAN5RMoAXty0RFUAAomkFyQADRcjSbAAI5Z

jC1QWzJ1y0uXnT2qAF4o7goUAgAHejQAC4sf0+8KKgApcVnJAAAHeRnsooMz8AA0rGAAHgU2KDM+DvJQYHcUlBsgRE+OQGT2fwN7ArAbfmOAPtYTnyU+jn3U/In+U/7nzc+qn3c/bc/U/gzZk/cvPE/DqogByAIz5fwKrHPn9k+E8VxBwWmbjbzcns1J9C9ifuC0p2dJVcn0c+CAeU/iny8/8n6XZyn/5jqnwTiafE8/XrKi/jnzkBa2dC/YX7E/

APvQ+/n7naoaj2iWukoAJuDw/iALxQZE0xl6XwdV6X1tsNfh4SAsiy/1rPwhiAHQ/sgAw/GfJtxqAoB96Xz2iOPs3DuX8gBcGZ4d6X6uzTvmK/toMGavlwg/9AD4Bbu+lkKvIg+Ru1JUI9tgBHbS5bnbWKb8yW7bkM94dOU5ts9sfbZ5Wvq/vbfeFcWQdjZc9HaIAG0AsgKqatUYwAL4OSBabIw9vvBQBZ3swBbEzGz/XyvSg333SmbltAhqla/X

XMntfbLvRggIvi1KNVl0wNQAQ38diAAOTUAdN/5z4mxpv7IGZAbXZiAJzI1QLiBpvqqcO4819t2OHsR7NN8iMNAB5vri7e5219ym2t88AZ1+uviNFs/Bnxpotl7+PV/Axgb5n+PYTHxZ6OCYvny35AVt/tvmZlB26ulvdsbtVP8d+TvwK2B2+O2zv6OAovpKCLv2d4iMZ1+x291+uEwKjRvof4uW5x/W2nbKhvdFqOv0/HHA82yzAx5FHvisDl9p

aAqvsQDavhJ8vvqxBqvnV8xPloHYWj99vv9+5JPqKpzd+2xIFhON3m+lNk2MfDEBbR+GQBOMPmlpPR7C2zQfwD9NTwODwf8jpSVS17hAwIDzYP/HSgcUDtJuB34sy0gSQAylP/NACu7DSoZPpqfdeA7E9djydzY/3G8dGvNGzlufCosotWgZj811Vj+nbMPGuufJ85d2tksftrOS3B1kHYtF/bJpe0if4c2ms8l8c2Q9MoHHSlHA8/o04kI4dyPs

eC+YDo+ZmnE6UsR6r/dayXoBMDBm4D+09v9/wP199fvvHYEfLiCmf0IcTW+DP+T4lPfAqD/b4az/+AQyDdeB83uBVz9todz8xgdD8SQXjpqA16zf/Dn7JgzT9hv6OCGs3c7Jg01n2VYO0xsjX6Nwu7wyvoy4JTxL9P0w1nP5Q3N/gPdAygWcACQLKBGf2cCJi2hGdU5AzJf9rOAfOe5IHFAGiwBR+VQB3HkmsMkpPbL8N7aiqGsmRPKNH0l9p/Jk

oQ0F8U4nL9sVWr+GsudMsXXym5smT9sf1HY+MvdAkAUrEeAArKaVQAAJhERQfRIiY8KIAAxeSUs1HW/+4JkAAEqaILgz/IAIz9ijhQC02XH5I/Tb+sUCuSAAPviGKAEt8bABZAABx6gAAX41C4A5nyBMNUb8U4pwtjftSfTfj8cwLmllZQCnGLfgcfHfkbGGf4z9ff7T+if4H/Gzr8ecf9MDcfs6q8ft7LETjL+i4oH91fzAFjfol+nfE79nf2J8

k/Rh+B7FDTvQWYioAQABcykmboIARRAACvWLH1Agge1apqACAz4kDv+1P9apoz85/0EHA6tJuv8tNlmVOfkvG33kAAaEal7M2yXjZB/feU/w+vj6ADJrH4N4NH5mkk2MmJkd8fQaCBrHPavX+cEy2UQAADFoABpWzWOUlE4qgACNjQACMmoABSWNnkUmGv8gAGyjFH5k+F794UQ79I/QACmio7/2XnuTeH8KBlAIegSkFJRAAFGxgABlXH393eQf

AD4c1T8IOqDQQQABeXgpRAAGTegACY5a/z2/44CIUJrgQAY4CAQA380/u4w4UJ38u/l7/QQQAB66RgYFKCn/r/MQZAADoKYF1K0gAATzDYmFPZv/3GEv+u/pDPaPmqeavgrgAfjC3OPqSp3eP5hPxIQDCgOmwA+KbOYAb7yB/if/4fFkD85V1x94fQBQATABLJt4CoAQAAbeYAAS6MAAkAmAAGm8mvFDaMYBwB7bBOmr313B/AOQE1CZyAov5ttT

//bZrnzB040cY+8ba9ZH/8v/6n0yDYbJ//sGs//JJDjRPr8VMX6eblM3dW+WFoAqnUvQDDQsoGoAP/8KwHyfC98bx3gA9056nxoOLt9YAAfHYWwUANYySJ87IGGiLKA+8APgPGBNmCS+KwF8tBMFAmxAAEKbQrQ75kPLW8x2FgwMeGwnXnHLFSc+LWV8HACAAOztXapZiC4gTgC0ANVAct8IcWrfNkEDsWQASmwY3397GNkDsQAAxuE3/xUeV6wZ

AO//Gwddfgk/UQkPfD/AHADEAL5QM4kHsn7fCMB0iw8CL/9In0bhYADxTT0aMADL0AgAqACYAI38bBplQXRqCsklAJMA1UAMALP+Tv5jAOefVAACAKIAkgCyAK1sWJ4q5wByZwCvAI22XgCQgJf/PEBEGmHfRmksHU8A16wXdn4AyJ9YQRwA/7wBP1v/Hul/HiSAm580gP//d583yXczTi1TvhwA4M0W6hJ8QAAA/TnuNOQnzEAAOlTNnwIoKiB5

n27QFAE+8EQMb/5jrC6/DfFe/wxufv8rP2QAFT8wfxQBFmxQvxIAXXtNtgB/B3FUv1VJRK5m/lQARHwpskAAFjkTBWooQD5GX0y/aOAqv022HK1zbDS/GYDIf3WA8YChv3lBPL8CvyK/Er8yvw6pehFJWUe8JV9lU0hjKxlebDMadr8fYwJ/cW1pbGkfMh9Xdn/fL981HykfYR8OH0ofel8qH0IfQR9FHxkfAEDVrCofGh9OQH5fatBGHyZ8AR8h

HyUfLB9OH24fZL92ABBApEDwQNd2MR8EQMkfD4DlH2JOCF5fgIJAkR9VHxQfBR90IFSfJJ9dAOj2Ap9zHxMfQJ8GQPCfMx9jH3CfXF8onzsfH2AHH2+fJJ8vHzZNLx9PH2FTdZML310fJnEAn3IBYJ8zHwvfRzIOQJsfLkCloFSfRJ8h/x/fRUDeQPSfE2MgX0hfGnwEX0E/VaxkXzCtQ59JP0/YDF8O7nxfGp9kMw5A2bhmn1afDp9unz6fAZ8h

nxGfZAxxnwO8KZ9ZnxxseZ9Fn0zkFZ81n2mfLZ8dn2mfPZ8KDAOfUp8rQMQMM59OYwwoAl9WnjDAt58HnxxfeMCvALjAq598gJaBT59Un3tAQV8I4B/AQF9aP0OfbtkwX0RqCF9FJ0muGF9BqjhfAp9bnz1AsQkF2VDtfF90Xxp8TF8jnwtAk6orQPxfDkDCfzLAkl8WgTJfAV8KXzeyeJMqXyU/aXxaX3G4el81gOOTZl8NflZfb/56X05fdawp

X15fWEDswLJZEV9x8U2AgHYFwJnjGcD1rF2ArG45Xy7A0yBFXw+gZV8LP0/fdV81e1wtLV8v3xa6PV8DX1y8I1986U/uKMDK3y1RaOAH3ybAEp9x32JOB18Y3wCtQCAO333fY35PX0Z8RX9ZoT9fAN97/wB2NN9oIIsgCN90wAyHHCcmwA0qeN8xLXNsJN9IchTfNN8s3yzfHN9obDzfLt9+2CLfdWVS31neIQCrATfA6GwRAKwCVABW33rfAN9G

31VzZt82QAnfbd8231BOICDPrgLfTACO/lT2Pt9AYAHfQwCAchiAyHFR3zNAzd8IrSXfQCDp31XfLX8AdlyfLd8V6R3fZd847SCJGNkN3xYg7U1pIIgAPd8Z3wPfCd5PwJauJJ8z32oAJADVqivfFT9F8TvfWl4jIKffW2BvgMvAhGNHIO/fW2Bn3x+tFyCYP2cfOz9CvFdcMD8nP0g/ZD83P1Q/ZCDSADg/Jmc4R0S7IKC/PxCgpydIp0FADD9g

v1HAjCgcP2vwE9h8Pwl8Ij8RJ3o/Uj8cwRy8Sj9+fho/UKC6P3xZBYdGP1PHNH9moAx/T8caWRR/WN9pPx4/BH96QIk/Ap9KoLCADH9yARagyBMcf0ag2T8acXk/V2xFP3ouSi1JgVU/IYD1P3bkTT8sbRE/eL95X0muYn9YfxaBHyCrwNqnTyDkAHUaCWxloIc/d3xwP3PHQKCVoH8/YSlhIC8/bG0fP2ighCBDoMC/YDoQv1OsML8IoNJxeGo4

IMegGL9wvxUqeL92Z1FfXh840SmA6l9sf2njaL8jgNy/VAB8v0K/Yr8EwFK/dqk6EXoMTcCnoMsgX796v2KgRr8owJa/bKk2v0BgmABOvwpxbr9ZWnKJcAtjXwLpQslngJG/XaE8fz9xG8cJvwMOKb9eoJm/LiA5vwW/a5kzck5aNb8Nv22/Xb99vyO/MudugJh/c79Lv2u/PChbvwe/J79Xvw+/L79vKnhg7IB/vwpxKmD0fyagmqCxaXGRQYDs

gAh/UidepyJ/aH9Tv0Wggtlf6Vx/OWCnHjqgtqD8RSag+Kd9gJ6gmWC+oKGA14C27H0/dWCSfxaBMn9Wfxn2aaBiu0D2On8Gf2Z/IL4HYPZ/Tn8nbh5/IkA+f0AzGrpBfxpNYX9RfwhyTQBJf2l/MX9CAFQoeX8ZfEV/ROBlfwwMVX91fxDjTX8dST7pHX9Vjj1/U/wDfxN/M39Lf1t/TP9O/zd/D39vf19/BmDafFIAQP9UsFD/CP92Xmj/I3hr

YnRAYmEOACT/NP8M/xnkKTBs/0AgPP8IAAL/Iv9i4Ir/Kv8a/1P8ev9G/xb/WeQ2/yb/Dv9T/Gd/Lv81mR7/NaDB/1CglroR/3dQMf8J/3Agsmxp/1n/cf9lIL+YJf9//1X/df8owMrgbf99/yP/RrwT/xjAc/9Hs0v/TyF5bFY/VfZYYKQgG+D4gMAAnS55AKnqD/834LyAvACVALbsbIDIgJauMwCTXwsAtqpwAOGiGwCWgFgArQCKnwxTWlog

EI/gpnweIPcAjgDf4IrALgC/wB8A4gDSAIteBQAKAOoA2gDGFnoAxgDmAMdeVgCBp3YA0BokEO4A52C+AIwQ1ACXAKiAz3NibBogtgASoOwacQDHXykAp+kIgOQQhr8yCUZpRQD34J//WpE1AO3gb4FEajgQ0UCRXn0Awd9Lcj/gk59TALOzV21wELMaSBDIAMhCWwCJEM72Bl5+EPQA1BCsANqyURDsEJgQ3wC8EOFsM14ggIdyUxCBY3CA9+Ca

DmiA1QDREMSAxhDcAJOfFID3ENyAhACMgPGuWhCfEKYQk59YXiAnIoDJrhKA0n8JmgqAqoDagPqAxoCttmy/NoCjfA6ArGCugLWggYCacS8nMYCAdgmA5DMfoJQOWYD5gNZyRrwlgJWAtSFJwP+gzbZNwMSgbYC9wOmAg8CTYMqQnJD0YLsgEGCzgPBgi4CoYMq/L6DTwLuApqk1EP6eT2MiYOoAS2DUFzXDTJok5nvoYxgOgn2vezg8Ny+rRu8S

YEXdRW1zWzK3Ujc8d2gYUkDEIDgfa8DegKQfCkCwQM+Aih9IQKBA+l9MQMOQ5R9jkNxAKEDlwM1qAaCmHxYfP4DkQIhA65DTkPRA/h9HkK2QnECnwLxAykCvkKJAqbwSQP+AlR8bo1+A6CBqQKH/MUD9H0ZAox9GvxZA0J82QNlApMDrH2ifNyCeQNCgpx8V4P5Atx9hQORTYIcvH1kQ4mxxAPFA1axwn0lAu3t4ELCfJFDUwMifeUDVQPUfb59l

QJXghUCGUIxQtJ8sUM1A/MDgXx1A+d8jQIKfA0D6wL5QxsC9AmbA7QCNWVqfZFCGnxtA/z02n06fHOgen36faDMnQL9gl0CeKAmfGZ85nwWfNOQ3jB9A1Z8Nn22fXZ99vH2fZDM+UPDAyMCDnxjA6sCYwMefSVCrUM7AlJ98wMzA3594QIBfIqC4oNIALJ9tQOznIsC/wBLAqF9TvnLA7zhKwN1A/lC6wNjAo59hUJOqUVD4EJtQmlDkwNefONDC

XyXtYl8KwNJfNSEBoMpfOeMRwIdxOl8NfgqQ3mpJX13AncCjfHnAzBlFwKLQ25CX6jhAoV9RIHXAxBETwK3AstDi0J9AOpDfoL+gum1oXgbQiyAzwN2QpAIAPxcgu8C2QS0gp20O9xdtfr9XwIP2C18AdiMgm18fwPtfTcBHXwAgl19ZILDfUCDvX19fOiCoIKnQxKBYIO3QiKAEIKjfJqd6oLbsNCDE3yYgZN9mAFTfWd5cIOzfLi5CIILfYiDc

lhLfMt84XyUAKiCOs1RZGt92IIYglekvUOxhKvElIMe8DiCZILdfbiC1X14guQsfMSOHQSCDAJ6ZQoD5IMSgMd8pIPYgqd9QMLkg9OD6hykQySCW3xQw1SDgILEgzbZNIMAwlSCY7TitSKkjIOFNU98y7Wp7cyDis0sg0aDrIPOBW2k7IKEA9yDe0LIgAD9nIPPA7V9kn1/fDyDuML6AoKdTx2Wg0D9HPwg/fmNfPwug2KDCxxEgDD8H7UQ/STC6

oC8g0KCroMw/ZKD4R1w/dKDAIAI/LKDapxyg0IAyP1IOfKCFCWo/TlDioMH+D3YyoPCnZlDhP2pgxH92P2R/SmDbMLNgo3F+P3sA1qDnMKqgpqDOoIE/QJ8DYIx/OT9+wIU/IcCs0OGg9/9rtkVgiWwJoKmg+H9ZoKPArmCNYJM/FeoQP34w9jDVX0vAjaDbP2SwmkcV5wpzAKCJMPOgpTD1oII+Tz8IoPuqYq5FMIQAS6CEoKC/GuoboOuA2L8V

Kki/PdDLIBeg+6D3oJART6CbgMALVtCCkMaQ434AYJaA44DgYNOAsGCIYPK/K4CYYJawhAtszgRg13YhEM0qFGDIETRgobDNY0xglAFsYN8pRql8YKmxXnxBv1Ww4mCiLlJg8b9Jv0B/OzDoFyR/Gll5vyuZJ2kkviUAZmDNvx2/ATY9vyN8Q78of1wtbmDNKl5gm78WKHu/R78CbGFgz79lLUYaDgtZsIlgjR5ckMzRHWCQf3lgt35IsOVgqacF

oLhfeH9m51m/Dj8nMIaglzDJJGNglWDasKxw0mD9sOyAOaDTICRw0n9ifnJ/R2Cqfxdg+n8OACZ/Fn9fYI5/AOCfYLZ/FVDGcMDgoX8Ff1Dg8X9UACl/cCDZf0kAWODhbHjgiyBE4OTg0/xM40MTNd9tfw4AXX8quH1/I39Tf1WOc39rfzt/TuDjgGLg938Dvy9/SP9OMj9/R7wA/yD/HIBa4J1w82wG4Nj/ZuDE/xT/dP9T/Ez/buDc/3z/WyhC

/2L/WeDS/yHg6v9k/1r/UOQG/2b/Vv92/zuMYuDu/yanboCbwMvA5eD3UNXgzjJR/zn/Fekt4ItsHeDdqD3gx7wD4IkAo+C1/w3/EfBd/0P/Y/9ecwneVIC74JjfFioH4Jv/TRhneBfgpBC5AKRgryof4LP/OxClWVfgmvClEOAQ7bDnwKpRSwDrAO0QmBC4APcQsVCEEOzncvDXAKMQ/CCqIFoQsxDCANwQ/wDOWkIQluwaALoAhgDrvCYAlgC2

AOZtdBCG8MwQ5QD7EMIABhDV8KCQ4BChAIfjT9DRANdcbhDJAMTOQD4DENf/SvD/dhEQxvDWnjrwkHDdEKkQzQDu8PgQ2kCU4KpgWDDXjkcQoADVEP6/WWoIEKsAqBCO8NgAh/DrDhcQm/DDEPAwtBD43jsQnBC/AJl8axDAF1W+OxCwgM3w8/CWEIKA2IC/tlcQ+vCn/3qfLxDt8JauCT8/ENT2AJCq/HfgkJCMiytghy4IkLtgqJDKgN2haoC6

gIaA1AAmgMSQ9oDx3g2wtJCBMMvAjJChgKyQwD5IcLWafJDNTkKQxYDlgJhHfNCYR2qQx6BakOlfepCSJyLjNSEiYNaQ0bDzgMhgir9rgJ7RX2xQEIJgz2wngPRgkZCpYLTgdjcB9zp5ZbohzzTaOE9k2nHPbo90AFr5Wp0ZQEzKH+Vcb1cPBc9c+igwBlgQcE9TSxVVCFDsaRJ4pFcSGVVu12xgQoRpVVAEfhV7FRsNCRw4jChGDTtHT1lDew1W

TyClRVdLrxM3bZcub2fPJtsH7xBnMQ8Wwx1XNlsExRFvN/0+wmGccsVrl1NXPsJ3uS+bbxAjRwe1F/lcz2zvKlVQHw0vLaNNT0trbU9/S3gbegAcAGwAIkNpr0NGV/RRuUuQLyVGiAi1KGR4hlk3ONVG8kXrfApccCBmbYhbaBRUXTddCGeiHy9TCGgSQxhUm19va881l0K1VI8aWxuPJ89zN0oVJ/dcj0FPF+9N3GmAU5cP9xfrDfVeCDrYCW9P

60IbA9coyA08FEBgH1zvZoigWyAHSBtf1QnPUR8qgFfiGABsAFD6OcRCAF2gOqBI5mOVHpJ+Sz2NSS9Lbxs4eBUTPAijKMJHiM2vIIZPxiSmA9Zz73dHNIi7z2VXG+9bj2O5Q1h7jwDHHI90lzyImM8uI1YcaYA9VxcrbsNnN3bLAB8DfXNDKW8XtxtQWkZjdAMGMVsz10aI1S9mggOQPsN9DwAtSoBEAA16bnBeZAgASfdiAGSAcUBiAHFAClgz

gDzALthPgFwAaYA6sGIAMJBiAD+AYgBZ+guAWfpiAFPIFUB3ADBgSthY9AnVQChsAHxActBh7ygPGzVJAE0APHh6AAHACYBaQ1PgcEiEIDb5RgZ8NHdCOQ1wtS3WIGQmWH4IJlhwHCoGbHABWBUIQYY0kEYkEqR9j3IKF4pUXWDIPyJgeTXFR300j0nXOttg7wyIwEY9lxWWJgVQZ0u3ZddqSP9oaYBG0w+PbNNvMH0GfdtenC4NfWs4PGJYI3kK

lxWrJW9MOzjKdY0oq2BbedsjJBFIiQAxSMMmCUjgSDjIbAA0HEWATQB14E0ACcjjgG1IojJ14CjIWfpbOwmAYciEEGwAc4ooBxxAE0izQnNI/RIrSMZgW0imlwpVIsVLMF7wNP4wYHHgaAAsECyAXNA8yE2ABgAPamniS69OaBcyJ8jBgHvCEQAhEE9AWqoPDhZvZIjrSlfI0TJ2hFqqKeBUiPTrP8j3yNqqFGB9xWbIUCi2SE/Izgp2416AdugJ

bSFMN8iYKMyAL8jCQHkgTuApQE74WG5GqnzQaCiAKLQoyK8zuQIo/3RaqkN4JltSKI/IzIBL0BNLKijwKNR3M/B6KMyAFGAtWwL3G8iGezAozIB64F0bF8jOKNQo61wN5W96fXBmKLc1aQNveh3lZCj/yLIozIALhCHrPyRvcD4olCjCKP0AFGBLEEN4I0ARKOYAHcjpQEo5VXkIelG5RIYJuQrGO2g1EB0o/ABt3CCGSUocUlZYYVteCBvIowA+

ahH4H3RcblEOTAgNyDLce0hRKIoo3HUXKxfItkASAAPCEQUigGKUQKi+gBtI7GQbyICo6PJkSDmNTpMVVFCo1JIt0CngUkAd0CrgpkAuIHDFIzxjrCyo6TAtuBBAVbZIwGFAN9Bi/RwOIKYwL2Oscqi8qLWcSggymh5AMH8/hHMAGoBAyhumVLB0KLBgVaIbyCAolJJrWDkYFfwIKOlAC1MQqP4o8HhxMFoovPsDaEriISBYwETgTXpn2HiojBdx

tCIACKifclDeRaieqEzgB1RerySKYUBCQFIADt91qMRIXaimADioy+NAkAHQLyi7AEiLDG48eDgAA+BYqLx4M6jZ1EswT7xGADB/UkA5qOUkA7YmYHSyUCB9AHkojqjlI1WMDWoSIGCANgIA3FxAPGBXqIQAd6izOGLXEKiiLk6TJAs8RQjAZ6i0SAKKAmoRMmyAcLB5qKeom8iX7hIAHdQZQFuo2h4IoFOo+t5FqMs0TABQaNL8e6jPpgm0CLRO

sHHccIB/iFTwFSAgAA==
```
%%