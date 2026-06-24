#include "LogicSystem.h"
#include <csignal>
#include <thread>
#include <mutex>
#include "AsioIOServicePool.h"
#include "CServer.h"
#include "ConfigMgr.h"
#include "RedisMgr.h"
#include "const.h"
#include "ChatServiceImpl.h"


int main(){
    auto& cfg = ConfigMgr::GetInstance();
    auto server_name = cfg["SelfServer"]["Name"];
    try{
        auto pool = AsioIOServicePool::GetInstance();
        //将登录数设为0
        RedisMgr::GetInstance()->HSet(LOGIN_COUNT,server_name,"0");
        //创建rpc服务器
        ChatServiceImpl service;
        grpc::ServerBuilder builder;
        std::string server_address = cfg["SelfServer"]["Host"] + ":" + cfg["SelfServer"]["RPCPort"];
        builder.AddListeningPort(server_address,grpc::InsecureServerCredentials());
        builder.RegisterService(&service);
        std::unique_ptr<grpc::Server> server(builder.BuildAndStart());
        //让其运行到另一个线程上
        std::thread grpc_server_thread([&server](){
            server->Wait();
        });

        boost::asio::io_context io_context;
        boost::asio::signal_set signals(io_context,SIGINT,SIGTERM);
        signals.async_wait([&io_context,pool,&server](const boost::system::error_code,int signal_number){
            io_context.stop();
            pool->Stop();
            server->Shutdown();
        });
        // std::string host = cfg["ChatServer"]["Host"];
        // unsigned short port = static_cast<unsigned short>(std::stoi(cfg["chatserver1"]["Port"]));
        unsigned short port = static_cast<unsigned short>(std::stoi(cfg["SelfServer"]["Port"]));
        auto chatServer = std::make_shared<CServer>(io_context,port);
        chatServer->Start();
        io_context.run();
        RedisMgr::GetInstance()->HDel(LOGIN_COUNT,server_name);
        RedisMgr::GetInstance()->Close();   //那其它池要不要关？？？？？？？？？？？？？？？？？？//
        grpc_server_thread.join();
    }catch (std::exception& e){
        std::cerr << "[ChatServer] error is :" << e.what() << std::endl;
    }
}