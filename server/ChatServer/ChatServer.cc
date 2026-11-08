#include <csignal>
#include <thread>
#include <mutex>
#include <iostream>
#include "LogicSystem.h"
#include "AsioIOServicePool.h"
#include "CServer.h"
#include "ConfigMgr.h"
#include "RedisMgr.h"
#include "const.h"
#include "ChatServiceImpl.h"
#include "Logger.h"
#include "MysqlMgr.h"
#include "CliOptions.h"
int main(int argc, char* argv[]){
    //通过cli输入，确定config 和 logs 路径
    Util::Options opt;
    std::string err;
    switch(Util::ParseArgs(argc, argv, opt, err)){
        case Util::ParseStatus::HelpRequested:
        Util::PrintUsage(std::cout, argv[0]);
        return 0;
        case Util::ParseStatus::Error:
            std::cerr << "error: " << err << "\n";      //这里用std::cerr, std::cout 主要是因为此时logger还未构建
            Util::PrintUsage(std::cerr, argv[0]);
            return Util::kExitUsage;
        case Util::ParseStatus::Ok:
            break;
        }

        if (!Util::ResolveConfig(opt, err)) {
            std::cerr << "error: " << err << "\n";
            return Util::kExitConfig;
        }
    ConfigMgr::SetConfigPath(opt.config.string());
    auto& cfg = ConfigMgr::GetInstance();
    auto server_name = cfg["SelfServer"]["Name"];
    const std::string tcp_port    = cfg["SelfServer"]["Port"];
    const std::string rpc_port    = cfg["SelfServer"]["RPCPort"];

    //检验 server_name,tcp_port,rpc_port    只要一个为空直接return
    if (server_name.empty() || tcp_port.empty() || rpc_port.empty()) {
        std::cerr << "error: 缺少 [SelfServer] 的 Name/Port/RPCPort: "
                  << opt.config << "\n";
        return Util::kExitConfig;
    }

    //防止端口号出现 "aaa"这种非法端口 而非"8080"这种正常端口
    auto parse_port = [](const std::string& s, int& out) {
        if (s.empty() || s.size() > 5) return false;
        //std::all_of 会遍历 s.begin() 到 s.end() 之间的每个字符，对每个字符执行后面的 lambda
        if (!std::all_of(s.begin(), s.end(), [](char c){ 
            return std::isdigit(static_cast<unsigned char>(c)); 
            })
        ) 
            return false;
        out = std::stoi(s);
        return out > 0 && out <= 65535;
    };
    int tcp_port_num = 0, rpc_port_num = 0;
    if (!parse_port(tcp_port, tcp_port_num) || !parse_port(rpc_port, rpc_port_num)) {
        std::cerr << "error: [SelfServer] Port/RPCPort 不是合法端口: " << tcp_port << " / " << rpc_port << "\n";
        return Util::kExitConfig;
    }

    boost::system::error_code ec;
    fs::path log_dir = opt.logDir.empty()? opt.config.parent_path() / "logs" : fs::absolute(opt.logDir, ec).lexically_normal();
    if (ec){
        std::cerr << "error: 日志目录非法\n"; 
        return Util::kExitConfig; 
    }
    Logger::GetInstance()->Init(server_name, LogLevel::Debug, true, log_dir.string());
    Defer log_guard([](){               //挪到try外，使得logger能够存活足够久
        Logger::GetInstance()->shutdown();  
    });



    
    try{
        MysqlMgr::GetInstance();
        if (!MysqlMgr::GetInstance()->Ping()){
            LOGE << "MySQL 不可达，节点启动中止（检查 [Mysql] 配置）";
            return Util::kExitStartup;
        }
        auto pool = AsioIOServicePool::GetInstance();
        //将登录数设为0
        if (!RedisMgr::GetInstance()->HSet(LOGIN_COUNT, server_name, "0")) {
            LOGE << "写入 LOGIN_COUNT 失败，节点启动中止";
            return Util::kExitStartup;
        }
        Defer hdel_count([&](){                                    // ← 紧跟 HSet，覆盖后面所有退出路径
            RedisMgr::GetInstance()->HDel(LOGIN_COUNT, server_name);
        });
        //的数据直接放到redis里即可（注意redis里的数据不会凭空消失，只能由外部进行修改，
        //创建rpc服务器
        ChatServiceImpl service;
        grpc::ServerBuilder builder;
        std::string server_address = cfg["SelfServer"]["Host"] + ":" + rpc_port;
        builder.AddListeningPort(server_address,grpc::InsecureServerCredentials());
        builder.RegisterService(&service);
        std::unique_ptr<grpc::Server> server(builder.BuildAndStart());
        
        if (!server) { 
            LOGE << "BuildAndStart() error"; 
            return Util::kExitStartup; 
        }
        //让其运行到另一个线程上
        std::thread grpc_server_thread([&server](){
            server->Wait();
        });
        /*为什么要defer ?
        std::thread 在 try 内声明	try 中抛异常走 catch，线程析构时仍 joinable() → std::terminate
        1.std::thread 的一个硬性规定（标准里写明）：
            如果 std::thread 对象在析构时仍然 joinable()（既没 join() 也没 detach()），析构函数调用 std::terminate()。
            joinable() 为真的条件很简单：线程已经启动，且还没被 join/detach。

            为什么标准要这么设计？因为析构时那个线程还在跑，只有两种处理方式，都很糟：
                自动 join()：析构会阻塞，而且如果线程永远不结束（比如 server->Wait()），程序就永远卡住；
                自动 detach()：线程变成“野线程”，它还在访问已经销毁的局部变量 → 未定义行为。
                既然两种都不安全，标准干脆规定“你必须自己决定”，不决定就终止整个进程。
        2.我明明写了 try...catch，为什么还是闪退”的答案。
            try 块内任意一行抛异常这行抛异常
            │
            ├─→ 开始栈展开（stack unwinding）
            │      │
            │      ├─ 逆序销毁 try 块里的局部对象：
            │      │    chatServer → signals → io_context → server → grpc_server_thread
            │      │                                                    ↑ 就是这一步
            │      │      此时 joinable == true → 析构函数 → std::terminate()
            │      │
            │      └─→ 进程直接 abort ❌
            │
            └─→ catch  永远轮不到执行    */
        Defer stop_grpc([&]{                            //一定要放在grpc_server_thread之后————> 分析析构顺序即可 
            if (server) server->Shutdown();               
            if (grpc_server_thread.joinable()) grpc_server_thread.join();
        });
        boost::asio::io_context io_context;
        boost::asio::signal_set signals(io_context,SIGINT,SIGTERM);
        signals.async_wait([&io_context,pool,&server](const boost::system::error_code,int){
            io_context.stop();
            pool->Stop();
            server->Shutdown();
        });
        // std::strng host = cfg["ChatServer"]["Host"];
        // unsigned short port = static_cast<unsigned short>(tcp_port_num);
        unsigned short port = static_cast<unsigned short>(tcp_port_num);
        auto chatServer = std::make_shared<CServer>(io_context,port);
        service.SetServer(chatServer);
        chatServer->Start();
        io_context.run();
        // RedisMgr::GetInstance()->Close();   //那其它池要不要关？ ——> 其它池有析构，你不用关心
    }catch (const std::exception& e) {
        LOGE << "[ChatServer] error is :" << e.what() << std::endl;
        return Util::kExitStartup;
    } catch (...) {
        LOGE << "[ChatServer] unknown exception" << std::endl;
        return Util::kExitStartup;
    }
}