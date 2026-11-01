
#include "CServer.h"
#include "Logger.h"
#include <iostream>
#include "HttpConnection.h"
#include "AsioIOServicePool.h"

CServer::CServer(boost::asio::io_context& ioc,unsigned short& port):_ioc(ioc),
    _acceptor(ioc,tcp::endpoint(tcp::v4(),port)),_socket(ioc){
    //  Start();   
}

// 这里并没有处理远端关闭可能出现的问题？？？ ————> 在此之前，需要明确：使用的HttpConnection是短链接，每个HttpConnection只负责 ————> 接收一个请求，
    //然后返回一个响应，然后断开连接          这里需要见void HttpConnection::Start()分析
void CServer::Start(){
    auto self = shared_from_this();
    auto& io_context = AsioIOServicePool::GetInstance()->GetIOService();
    std::shared_ptr<HttpConnection> new_con = std::make_shared<HttpConnection>(io_context);
    //_socket : 要给客人提供服务的服务员
    _acceptor.async_accept(new_con->GetSocket(),[new_con,self](beast::error_code ec){
        try{    
            //if 有错误，直接掠过，为下一个连接提供服务
            if(ec){
                LOGE << "_acceptor 接收出现错误，现接收下一个 " << std::endl;
                self->Start();
                return;
            }
            // std::make_shared<HttpConnection>(std::move(self->_socket))->Start();
            new_con->Start();
            self->Start();
        }catch(std::exception& e){
            LOGE << "Exception is " << e.what() << std::endl;
            self->Start();
        }
    });
}