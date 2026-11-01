#include <iostream>
#include <json/json.h>
#include <json/value.h>
#include <json/reader.h>
#include "const.h"
#include "ConfigMgr.h"
#include "hiredis.h"
#include "RedisMgr.h"
#include "MysqlMgr.h"
#include "AsioIOServicePool.h"
#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <boost/asio.hpp>
#include "StatusServiceImpl.h"
#include "Logger.h"

/*步骤 4 执行流程  StatusServer 选择一个 ChatServer，生成并存储 token，返回给 GateServer 
═══════════════════════════════════════════════════════════════════════════════

GateServer 发起 gRPC 调用
    │
    │  stub->GetChatServer(&context, request, &reply)
    │
    ▼
┌────────────────────────────────────────────────────────────────────────────┐
│  StatusServiceImpl::GetChatServer()                                        │
│  StatusServiceImpl.cpp 第 23~32 行                                         │
└────────────────────────────────────────────────────────────────────────────┘
    │
    ├── 1. 接收 GateServer 发来的请求
    │     request->uid()  = 用户ID (例如 42)
    │
    ├── 2. 调用 getChatServer() 选择一台服务器
    │     │
    │     ▼
    │   ┌─────────────────────────────────────────────────────────────────┐
    │   │  getChatServer()                                      第63~85行 │
    │   │                                                                │
    │   │  ① std::lock_guard<std::mutex> guard(_server_mtx);    ← 加锁  │
    │   │                                                               │
    │   │  ② auto minServer = _servers.begin()->second;        ← 默认  │
    │   │     // _servers 内容：                                         │
    │   │     //   "chatserver1" → { host:"127.0.0.1", port:"8990" }    │
    │   │     //   "chatserver2" → { host:"127.0.0.1", port:"8991" }    │
    │   │                                                               │
    │   │  ③ 注释了负载均衡逻辑（从 Redis 读取每个 ChatServer           │
    │   │     的当前连接数 con_count，选出最小的）                       │
    │   │                                                               │
    │   │  ④ return minServer;  // 返回一个 ChatServer 对象             │
    │   └─────────────────────────────────────────────────────────────────┘
    │     │
    │     │  server = { host:"127.0.0.1", port:"8990", name:"chatserver1" }
    │     │
    │     ▼
    ├── 3. 把 ChatServer 信息填入 gRPC 回复体 (reply)
    │     │
    │     │  reply->set_host(server.host);         // "127.0.0.1"
    │     │  reply->set_port(server.port);         // "8990"
    │     │  reply->set_error(ErrorCodes::Success); // 0
    │     │
    │     ▼
    ├── 4. 生成 UUID token，填入回复体
    │     │
    │     │  reply->set_token(generate_unique_string());
    │     │  │
    │     │  ▼
    │     │  ┌──────────────────────────────────────────────────────────┐
    │     │  │  generate_unique_string()                       第13~18行│
    │     │  │                                                          │
    │     │  │  boost::uuids::uuid uuid =                               │
    │     │  │      boost::uuids::random_generator()();                 │
    │     │  │                                                          │
    │     │  │  // 生成随机 UUID，例如：                                 │
    │     │  │  // "f47ac10b-58cc-4372-a567-0e02b2c3d479"               │
    │     │  │                                                          │
    │     │  │  return to_string(uuid);                                 │
    │     │  └──────────────────────────────────────────────────────────┘
    │     │
    │     ▼
    ├── 5. 把 uid→token 存入 Redis（后续 ChatServer 验证用）
    │     │
    │     │  insertToken(request->uid(), reply->token());
    │     │  │
    │     │  ▼
    │     │  ┌──────────────────────────────────────────────────────────┐
    │     │  │  insertToken()                                 第118~123行│
    │     │  │                                                         │
    │     │  │  ① 把 uid 转成字符串                                     │
    │     │  │     std::string uid_str = std::to_string(uid);          │
    │     │  │     // uid=42 → uid_str = "42"                          │
    │     │  │                                                        │
    │     │  │  ② 拼接 Redis 的 key                                   │
    │     │  │     std::string token_key = USERTOKENPREFIX + uid_str; │
    │     │  │     // USERTOKENPREFIX = "utoken_"                     │
    │     │  │     // → token_key = "utoken_42"                       │
    │     │  │                                                        │
    │     │  │  ③ 写入 Redis                                          │
    │     │  │     RedisMgr::GetInstance()->Set(token_key, token);    │
    │     │  │     // Redis 中：                                      │
    │     │  │     //   key   = "utoken_42"                            │
    │     │  │     //   value = "f47ac10b-58cc-4372-a567-0e02b2c3d479"│
    │     │  └──────────────────────────────────────────────────────────┘
    │     │
    │     ▼
    └── 6. 返回 gRPC 响应给 GateServer
          │
          │  return Status::OK;
          │
          ▼
GateServer 收到回复
    │
    │  reply 中的内容（gRPC 反序列化后）：
    │
    │   ┌─────────────────────────────────────┐
    │   │  .host()  = "127.0.0.1"             │  客户端该连哪个 IP
    │   │  .port()  = "8990"                  │  客户端该连哪个端口
    │   │  .token() = "f47ac10b-..."          │  登录凭证
    │   │  .error() = 0                       │  成功
    │   └─────────────────────────────────────┘
    │
    ▼
LogicSystem 把 reply 包装成 HTTP JSON，返回给客户端
    │
    │  根节点格式：
    │  {
    │    "error" : 0,
    │    "user"  : "xxx",
    │    "uid"   : 42,
    │    "token" : "f47ac10b-58cc-4372-a567-0e02b2c3d479",
    │    "host"  : "127.0.0.1"
    │  }
    │
    ▼
客户端收到后，用 host:port 连接 ChatServer，用 token 做身份验证*/

void RunServer(){
    auto& cfgMgr = ConfigMgr::GetInstance();
    std::string service_addr = cfgMgr["StatusServer"]["Host"] + ":" + cfgMgr["StatusServer"]["Port"];
    StatusServiceImpl service;
    grpc::ServerBuilder builder;
    // 监听端口和添加服务
    builder.AddListeningPort(service_addr,grpc::InsecureServerCredentials());
    builder.RegisterService(&service);
    // 构建并启动gRPC服务器
    std::unique_ptr<grpc::Server> server(builder.BuildAndStart());
    LOGI << "Server listening on " << service_addr << std::endl;
    //创建用于监听停止信号的io_context ————> 这里有一个问题？为什么StatusServer不需要io_context？ ——> 作为rpc服务，伪客户端和伪服务端
    //共享一个io_context ，此io_context由伪客户端创建
    boost::asio::io_context ioc;
    boost::asio::signal_set signals(ioc,SIGINT,SIGTERM);
    signals.async_wait([&server](const boost::system::error_code& error, int signal_number){
        if (!error) {
            LOGI << "Shutting down server..." << std::endl;
            server->Shutdown(); // 优雅地关闭服务器
        }
    });
    std::thread t([&ioc](){
        ioc.run();
    });
    server->Wait(); // 等待服务器关闭
    ioc.stop();
    t.join();   //等待ioc使用完毕
}

int main(int argc, char** argv) {
    Logger::Init("statusserver", LogLevel::Debug, true, "logs");
    try {
        RunServer();
    }
    catch (std::exception const& e) {
        LOGE << "Error: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }
    Logger::Init("chatserver1", LogLevel::Debug, true, "logs");
    return 0;
}

/*
bool bstop = false;
std::condition_variable cond_quit;
std::mutex mutex_quit;
int main(){
    try{
        std::cout << "Server run -------------------- " << std::endl;
        auto pool = AsioThreadPool::GetInstance();
        //这个io_context只负责监听信号
        boost::asio::io_context io_context; 
        //注册信号
        boost::asio::signal_set signals(io_context,SIGINT,SIGTERM);
        signals.async_wait([&pool,&io_context](const boost::system::error_code& error, int signal_number){
            io_context.stop();
            pool->Stop();
            std::unique_lock<std::mutex> unique_lock(mutex_quit);
            bstop = true;
            cond_quit.notify_one();
        });
        std::thread sig_thread([&io_context](){
            io_context.run();
        }); 
        Server s(pool->GetIOservice(),8080);
        //让主线程阻塞
        {
            std::unique_lock<std::mutex> unique_lock(mutex_quit);
            while(!bstop){
                cond_quit.wait(unique_lock);
            }
        }      
        if(sig_thread.joinable()){
            sig_thread.join();
        }
    }catch (std::exception& e){
        std::cerr << "Exception :" << e.what() << std::endl;
    }
}
*/