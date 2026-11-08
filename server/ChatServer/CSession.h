#pragma once
#include "const.h"
#include <boost/asio.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast.hpp>
#include <boost/asio.hpp>
#include <queue>
#include <memory>
#include <functional>

namespace beast = boost::beast;         // from <boost/beast.hpp>
namespace http = beast::http;           // from <boost/beast/http.hpp>
namespace net = boost::asio;            // from <boost/asio.hpp>
using tcp = boost::asio::ip::tcp;       // from <boost/asio/ip/tcp.hpp>

class SendNode;
class MsgNode;
class CServer;
class RecvNode;

// 单个 session 的异步读写回调确实在固定的一个线程上执行 ||| 多线程获取对应的io_context
class CSession: public std::enable_shared_from_this<CSession>{
public:
    CSession(boost::asio::io_context& ioc,std::shared_ptr<CServer>);
    ~CSession();
    tcp::socket& GetSocket();
    std::string& GetSessionId();
    void SetUserId(int uid);
    std::size_t GetUserId();
    std::shared_ptr<CServer> GetServer();
    void Start();  
    void Send(short msg_id,std::string msg_data);
    void Close();
private:
    using handler = std::function<void(const boost::system::error_code& ec, std::size_t bytesTransfered)>;
    void AsyncReadHead();
    void AsyncReadBody(int msg_len);
    void AsyncReadFull(std::size_t len,handler);
    void AsyncReadLen(std::size_t has_read_len, std::size_t total_len,handler);
    void HandleWrite(const boost::system::error_code& error, std::shared_ptr<CSession> shared_self);
    // void DealExceptionSession();
    std::string _session_id;
    std::size_t _user_uid;
    tcp::socket _socket;
    std::shared_ptr<CServer> _server;

    //_data是实际的异步读写场所 _recv_head_node和_recv_msg_node 只负责接收数据
    char _data[MAX_LENGTH];
    // bool _b_close;
    //session 锁
    /*我的问题是一个session不是只占一个线程吗？为什么还有用锁呢？
    那锁是给谁加的？—— 给"别的线程"加的
        Close() 的调用方不止 owner 线程。你项目里现在有 3 个调用点：

        调用点	                                        所在线程	    是否 owner
        CSession::AsyncReadHead 的 ec 分支	            池线程	        ✅ 是
        CSession::AsyncReadBody 的 ec 分支	            池线程	        ✅ 是
        CServer::ClearSession 里的 session->Close()	    谁调谁就是	    ❌ 可能不是
    */
	std::mutex _session_mtx;
    /*如果没有锁，两个线程同时进 Close()：一个在 _socket.close()，另一个也 _socket.close()，虽然不会崩溃但不够干净。
    _session_mtx 保证同一时刻只有一个线程在执行关闭操作。*/
    std::queue<std::shared_ptr<SendNode>> _send_que;
    std::mutex _send_lock;
    //收到的消息结构
    std::shared_ptr<RecvNode> _recv_msg_node;
    // bool _b_head_parse;
    //收到的头部结构
    std::shared_ptr<MsgNode> _recv_head_node;

};

class LogicSystem;
class LogicNode{
    friend class LogicSystem;
public:
    LogicNode(std::shared_ptr<CSession> csession,std::shared_ptr<RecvNode> recvNode);
private:
    std::shared_ptr<CSession> _csession;
    std::shared_ptr<RecvNode> _recvNode;
};
