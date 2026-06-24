#pragma once
#include "const.h"
#include <map>

class HttpConnection: public std::enable_shared_from_this<HttpConnection>{
    friend class LogicSystem;
public:
    HttpConnection(boost::asio::io_context& ioc);
    ~HttpConnection();
    void Start();
    tcp::socket& GetSocket();
private:
    void CheckDeadline();
    void HandleReqest();
    void WriteResponse();
    void PreParseGetParam();
    tcp::socket _socket;
    beast::flat_buffer _buffer; //用来接收数据
    http::request<http::dynamic_body> _request; //表示请求体，用来解析请求(任何请求)
    http::response<http::dynamic_body> _response; //表示响应体，用来回应给客户端响应(任何响应)
    //-------------要注意这是初始化 在 C++ 中，__非静态数据成员的类内初始化不允许使用圆括号 ————> 可能被解析为函数声明
    // net::steady_timer deadline_(_socket.get_executor(), std::chrono::seconds(60));  
    net::steady_timer deadline_{_socket.get_executor(), std::chrono::seconds(60) };         //用来做定时器判断请求是否超时
    std::string _get_url;
    std::unordered_map<std::string,std::string> _get_params;
    // std::map<std::string,std::string> _get_params;
};