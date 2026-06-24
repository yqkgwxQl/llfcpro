#include "CServer.h"
#include <iostream>
#include "const.h"
#include "ConfigMgr.h"
#include "AsioIOServicePool.h"
#include "RedisMgr.h"
#include <assert.h>

void TestRedisMgr() {
    // assert(RedisMgr::GetInstance()->Connect("127.0.0.1", 6380));
    // assert(RedisMgr::GetInstance()->Auth("233233"));
    assert(RedisMgr::GetInstance()->Set("blogwebsite","llfc.club"));
    std::string value="";
    assert(RedisMgr::GetInstance()->Get("blogwebsite", value) );
    assert(RedisMgr::GetInstance()->Get("nonekey", value) == false);
    
    assert(RedisMgr::GetInstance()->HSet("bloginfo","blogwebsite", "llfc.club"));
    assert(RedisMgr::GetInstance()->HGet("bloginfo","blogwebsite") != "");
    assert(RedisMgr::GetInstance()->ExistsKey("bloginfo"));
    assert(RedisMgr::GetInstance()->Del("bloginfo"));
    // assert(RedisMgr::GetInstance()->Del("bloginfo"));    这里会有作者写的其实返回true,我写的返回false 所以会使程序终止
    assert(RedisMgr::GetInstance()->ExistsKey("bloginfo") == false);
    assert(RedisMgr::GetInstance()->LPush("lpushkey1", "lpushvalue1"));
    assert(RedisMgr::GetInstance()->LPush("lpushkey1", "lpushvalue2"));
    assert(RedisMgr::GetInstance()->LPush("lpushkey1", "lpushvalue3"));
    assert(RedisMgr::GetInstance()->RPop("lpushkey1", value));
    assert(RedisMgr::GetInstance()->RPop("lpushkey1", value));
    assert(RedisMgr::GetInstance()->LPop("lpushkey1", value));
    assert(RedisMgr::GetInstance()->LPop("lpushkey2", value)==false);
    RedisMgr::GetInstance()->Close();
}

int main(){
    // TestRedisMgr();
    ConfigMgr& gCfgMgr = ConfigMgr::GetInstance();
    std::string gate_port_str = gCfgMgr["GateServer"]["Port"];
    unsigned short gate_port = atoi(gate_port_str.c_str());
    try{    
    /*这个 1 是一个并发提示（concurrency hint），它告诉 io_context：预计最多只有 1 个线程会同时调用 run() 或 poll()
        io_context 内部维护着一个任务队列（存放所有待执行的异步回调）。当多个线程同时调用 ioc.run() 时，它们会竞争从队列中取出任务来执行，这就需要加锁来保证线程安全。
        而当你传入 1 时，io_context 会做以下优化：
        跳过锁操作：既然只会有一个线程在跑 event loop，内部队列就不需要加互斥锁（mutex），减少了同步开销。*/
        net::io_context ioc{1};
        // unsigned short port = static_cast<unsigned short>(8080);
        boost::asio::signal_set signals(ioc,SIGINT,SIGTERM);
        signals.async_wait([&ioc](boost::system::error_code ec, int signal_number){
            boost::ignore_unused(signal_number); 
            if(ec){
                return;
            }
            ioc.stop(); //由于有 _acceptor.async_accept,所以这里会阻塞
            // AsioIOServicePool::GetInstance()->Stop(); //停止线程池中的所有io_context
        });
        std::make_shared<CServer>(ioc,gate_port)->Start();
		std::cout << "Gate Server listen on port: " << gate_port << std::endl;
        ioc.run();
    }catch(std::exception& e){
        std::cerr << "Exception is :" << e.what() << std::endl;
        return EXIT_FAILURE; //返回错误码
    }
}