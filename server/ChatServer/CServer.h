#pragma once

#include "const.h"   
#include <string>
#include <memory>
#include <boost/asio.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast.hpp>
#include <boost/asio.hpp>
#include <map>
#include <mutex>

namespace beast = boost::beast;         // from <boost/beast.hpp>
namespace http = beast::http;           // from <boost/beast/http.hpp>
namespace net = boost::asio;            // from <boost/asio.hpp>
using tcp = boost::asio::ip::tcp;       // from <boost/asio/ip/tcp.hpp>

class CSession;
class CServer: public std::enable_shared_from_this<CServer>{
public:
    CServer(boost::asio::io_context& ioc,unsigned short& port);
    ~CServer();
    void ClearSession(std::string session_id);
    void Start();
private:
    void StartAccept();
    void HandleAccept(std::shared_ptr<CSession> new_session,const boost::system::error_code& error);

    //为什么源码中没有host 因为我是服务器，有自己的Host没用呀，我要监听本机端口的任意连接
    net::io_context& _io_context;
    // std::string _host;
    short _port;
    tcp::acceptor _acceptor; 
    ///这个由session_id 和 csession 构成的_sessions有什么作用，它的作用会被UseMgr取代吗????????????????
    std::map<std::string,std::shared_ptr<CSession>> _sessions;
    std::mutex _mutex;
};