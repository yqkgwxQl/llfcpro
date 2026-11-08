
#include "CServer.h"
#include "Logger.h"
#include <iostream>
#include "AsioIOServicePool.h"
#include "CSession.h"
#include "UserMgr.h"
#include "RedisMgr.h"
#include "ConfigMgr.h"

/*  两个完全不是同一个东西。

对比维度	        GateServer 的 CServer	                        ChatServer 的 CServer
用途	            监听 HTTP 连接	                                监听 TCP长连接（聊天socket）
连接管理	    创建 HttpConnection，无状态，一请求一响应	        创建 CSession，需要维护连接状态
session 管理	        ❌ 无	                                ✅ _sessions map 跟踪所有活跃连接
心跳检测	            ❌ 无	                                ✅ steady_timer 每 60 秒检查心跳，过期断开
Redis 记录连接数	    ❌ 无	                                ✅ HSet(LOGIN_COUNT, self_name, count)
清除过期连接	        ❌ 无	                                ✅ ClearSession() + DealExceptionSession()
accept 回调	        简单的 lambda，继续监听	                      调用 HandleAccept，记录 session 到 map
一句话：
GateServer::CServer  → 简单的 HTTP 服务器，接收请求就完事
ChatServer::CServer  → 完整的 TCP 长连接服务器，有 session 管理、心跳、Redsi 上报

ChatServer 是聊天服务端，客户端 TCP 连上来后要保持长连接，双向通信。ChatServer 必须记住每个客户端
主要区别在于管理_sessions;
*/
// 这里并没有处理远端关闭可能出现的问题？？？ ————> 在此之前，需要明确：使用的HttpConnection是短链接，每个HttpConnection只负责 ————> 接收一个请求，
    //然后返回一个响应，然后断开连接          这里需要见void HttpConnection::Start()分析


CServer::CServer(boost::asio::io_context& ioc,unsigned short& port) //这里我不需要host呀，只需要监听来自该端口的任何信号即可，客户端才需要ip+port;
    :_io_context(ioc),_port(port),_acceptor(_io_context,tcp::endpoint(tcp::v4(),_port)){
    
    // StartAccept();
    LOGI << "Server start success, listen on port : " << _port << std::endl;
}

CServer::~CServer(){
    LOGI << "Server destruct listen on port : " << _port << std::endl;
}
void CServer::Start(){
    StartAccept();
}
// auto self = enable_shared_from_this();
//这里为什么用this,不用self,因为initHttpHandlers在构造函数内部，此时此对象并未构造完毕，因此不能使用智能指针
void CServer::StartAccept(){
    auto self = shared_from_this();
    auto& ioc = AsioIOServicePool::GetInstance()->GetIOService();
    auto new_session = std::make_shared<CSession>(ioc,self);
    // auto new_session = std::make_shared<CSession>(ioc,this);
    _acceptor.async_accept(new_session->GetSocket(),std::bind(&HandleAccept,this,new_session,std::placeholders::_1));
}

void CServer::HandleAccept(std::shared_ptr<CSession> new_session,const boost::system::error_code& error){
    if(!error){
        new_session->Start();   //这里是异步的，不会阻塞
        std::lock_guard<std::mutex> guard_lock(_mutex);
        _sessions.insert(std::pair(new_session->GetSessionId(),new_session));
    }else{
        LOGI << "session accept failed, error is " << error.what() << std::endl;
        //这里出错实际上是Server 自身关闭、文件描述符耗尽、acceptor 被销毁等，不过出错了不用关心，引用计数归零自动销毁
    }
    StartAccept();
}

//当有连接异常时，可以调用移除用户Session的接口
void CServer::ClearSession(std::string session_id){
    std::shared_ptr<CSession> session;
    {
        std::lock_guard<std::mutex> guard_lock(_mutex); 
        auto session_iter = _sessions.find(session_id);
        if(session_iter == _sessions.end())
            return;     //这里已经清过了

        session = _sessions[session_id];
        _sessions.erase(session_id);                     //------------清理ChatServer对应的_session[session_id]信息
    }
    if(!session)
        return;
    const std::size_t uid = session->GetUserId();
    /*这里是有问题的，Close()是关闭对应Session的_socket, 是跨线程操作，干脆直接到各自的线程上再Close() 
    eg:Session::AsyncReadHead 的 ec 分支、CSession::AsyncReadBody 的 ec 分支、*/
    // session->Close();                                           //------------------关闭对应的文件描述符
    if(uid == 0){
        LOGI << "空连接关闭: session_id=" << session_id;   // 没登录过的连接
        return;
    }
    if(UserMgr::GetInstance()->RmvUserSession(uid, session->GetSessionId())){ //------------清理UserMgr对应的_uid_to_session[uid]信息
        //------------清理Redis中对应的临时的session信息  
        std::string ipKey = USERIPPREFIX + std::to_string(uid);
        auto& config = ConfigMgr::GetInstance();
        std::string server_name = config["SelfServer"]["Name"];
        RedisMgr::GetInstance()->Del(ipKey);
        RedisMgr::GetInstance()->HIncrBy(LOGIN_COUNT, server_name, -1);
    }
    
    LOGI << "session closed: uid=" << uid << " session_id=" << session_id;
}   