#include "HttpConnection.h"
#include "Logger.h"
#include <iostream>
#include "const.h"
#include "LogicSystem.h"

HttpConnection::HttpConnection(boost::asio::io_context& ioc):_socket(ioc){

}

HttpConnection::~HttpConnection(){
    
}

tcp::socket& HttpConnection::GetSocket(){
    return _socket;
}
// 如果我们输入带参数的url请求`http://localhost:8080/get_test?key1=value1&key2=value2`

// 会收到服务器反馈`url not found`

// 所以对于get请求带参数的情况我们要实现参数解析，我们可以自己实现简单的url解析函数

//ToHex ——> 数值转字符（编码）
// 功能： 将一个 0~15 的整数（即 4 bit 的二进制数），转换为对应的十六进制字符 '0'-'9' 或 'A'-'F'。
unsigned char ToHex(unsigned char x){
    return x > 9 ? x + 55 : x + 48;
}
// unsigned char 它是为了防止在处理非英文字符或特殊二进制数据时，因为负数补码机制导致的数据错乱。

//FromHex ——> 字符转数值（解码）
// 功能： 将十六进制字符 '0'-'9', 'a'-'f', 'A'-'F' 还原为对应的 0~15 的整数值。它是 ToHex 的逆操作。
unsigned char FromHex(unsigned char x){
    unsigned char y = 0;
    if(x >= 'A' && x <= 'Z')
        y = x -'A' + 10;
    if(x >= 'a' && x <= 'z')
        y = x -'a' + 10;   
    if(x > '0' && x <'9')
        y = x - '0';
    else
        assert(0);  //报错
    return y;
}

//url 编码工作
//它的核心作用是：将一个普通的字符串转换为可以在 HTTP URL 中安全传输的格式。因为在 URL 中，某些字符（如空格、中文、特殊符号）
            //具有特殊含义或是不被允许的，必须转换成 %XX 的形式。
std::string UrlEncode(const std::string& str){
    std::string strTemp = "";
    std::size_t length = str.length();
    for(std::size_t i = 0; i < length; i++){
        //判断是否仅有数字和字母构成
        if(isalnum((unsigned char)str[i]) || str[i] == '-' || str[i] == '_' || str[i] == '.' || str[i] == '~')
            strTemp += str[i];
        else if(str[i] == ' ')
            strTemp += '+';
        else {
            //其他字符需要提前加%并且高四位和低四位分别转为16进制
            strTemp += '%';
            strTemp += ToHex((unsigned char)str[i] >> 4); // 取高4位
            strTemp += ToHex((unsigned char)str[i] & 0x0F); // 取低4位
        }
    }
    return strTemp;
}
/*  假设输入字符是感叹号 !。
    查表：! 的 ASCII 码是 33（十进制）。
    拆分：
    33 的二进制是 0010 0001。
    高 4 位：0010 → 十进制 2。
    低 4 位：0001 → 十进制 1。
    转换：
    ToHex(2) → '2'。
    ToHex(1) → '1'。
    拼接：% + '2' + '1' = %21*/
std::string UrlDecode(const std::string& str){
    std::string strTemp = "";
    std::size_t length = str.length();
    for(std::size_t i = 0; i < length; i++){
        if(str[i] == '+')
            strTemp += ' ';
        else if(str[i] == '%'){
            //i + 2 <= length - 1
            assert(i + 2 < length);
            unsigned char high = FromHex((unsigned char)str[++i]);
            unsigned char low = FromHex((unsigned char)str[++i]);
            strTemp += high*16 + low;
        }else{
            strTemp += str[i];
        }
    }
    return strTemp;
}


void HttpConnection::Start(){
    auto self = shared_from_this();
    //这里回调函数绝对不能传引用beast::error_code& ec 否则，可能ec 早早被析构了
    http::async_read(_socket,_buffer,_request,[self](beast::error_code ec,std::size_t bytes_transferred){
        try{
            if(ec){
                LOGI << "http read err is " << ec.what() << std::endl;
                return;
            }
            boost::ignore_unused(bytes_transferred); 
            //我觉得这样写才对  √√√√√√√√√√√√√√√√
            self->CheckDeadline();
            self->HandleReqest();
            //因为我使用http请求设置为短链接，所以每个HttpConnection只负责 ————> 接收一个请求，然后返回一个响应，然后断开连接
        }catch (std::exception& e){
            LOGE << "Exception is " << e.what() << std::endl;
        } //这里并没有处理远端关闭可能出现的问题？ ————>  if远端关闭了，说明远端并未发送请求，直接执行异常逻辑，没有问题
    });
}

void HttpConnection::CheckDeadline(){
    auto self = shared_from_this();
    //60s后才触发回调
    deadline_.async_wait([self](beast::error_code ec){
        if(!ec){
            //if 没错误并且60s还没有处理完请求，关闭_socket
            self->_socket.close(ec);
        }   
    });
}

void HttpConnection::HandleReqest(){
    _response.version(_request.version());
    //这里设置为短链接
    _response.keep_alive(false);

    //method 请求方式
    if(_request.method() == http::verb::get){
        PreParseGetParam();
        bool success = LogicSystem::GetInstance()->HandleGet(_get_url,shared_from_this());
        if(!success){
            //result是状态码和状态码描述
            _response.result(http::status::not_found);
            _response.set(http::field::content_type,"text/plain");
            //它的作用类似:bool LogicSystem::HandleGet(std::string path, std::shared_ptr<HttpConnection> con);
    //都是往body里写数据：beast::ostream(connection->_response.body()) << "receiver get_test req"; or beast::ostream(_response.body()) << "url not found\r\n";
            beast::ostream(_response.body()) << "url not found\r\n";
            WriteResponse();
            return;
        }
        _response.result(http::status::ok);
        _response.set(http::field::server,"GateSever");
        WriteResponse();
        return;
    }

    if(_request.method() == http::verb::post){
        //这里不用再PreParseGetParam(); 因为post请求是不包含路径的
        bool success = LogicSystem::GetInstance()->HandlePost(_request.target(),shared_from_this());
        if(!success){
            _response.result(http::status::not_found);
            _response.set(http::field::content_type,"text/plain");
            beast::ostream(_response.body()) << "url not found";
            WriteResponse();
            return;
        }
        _response.result(http::status::ok);
        _response.set(http::field::server,"GateServer");
        WriteResponse();
        return;
    }
}

//这里只是在response的报头里设置content_length ，然后发送response，设置回调(服务器断开连接)
void HttpConnection::WriteResponse(){
    auto self = shared_from_this();
    _response.content_length(_response.body().size());
    http::async_write(_socket,_response,[self](beast::error_code ec,std::size_t bytes_transferred){
        boost::ignore_unused(bytes_transferred); 
        self->_socket.shutdown(tcp::socket::shutdown_send,ec);
        self->deadline_.cancel();
    });
}

//解析之前获取参数
void HttpConnection::PreParseGetParam(){
    //http://localhost:8080/get_test?key1=value1&key2=value2
    //提取 URI
    auto uri = _request.target();
    //查找查询字符串的开始位置（即？’的位置）
    auto query_pos = uri.find('?');
    if(query_pos == std::string::npos){
        _get_url = uri;
        return;
    }
    // key1=value1&key2=value2
    _get_url = uri.substr(0,query_pos);
    std::string query_string = uri.substr(query_pos + 1);
    std::string key;
    std::string value;
    std::size_t pos;

    while((pos = query_string.find('&')) != std::string::npos){
        // key1=value1
        auto pair = query_string.substr(0,pos);
        std::size_t eq_pos;
        if((eq_pos = pair.find('=')) != std::string::npos){
            key = UrlDecode(pair.substr(0,eq_pos));
            value = UrlDecode(pair.substr(eq_pos + 1));
            /*  _get_params.insert(std::make_pair(key,value));
                _get_params[key] = value;
                这两句代码：在key值不存在时效果相同，都是插入一个(key,value)
                            在key值存在时 效果不同。 前者：保留原有的value , 后者：替代原有的value，改用新的value
                --这里根据使用场景的不同，选用不同的方法
            */
           _get_params[key] = value;
        }
        query_string.erase(0,pos + 1);
    }
    //没找到'&'，即代表最后一项key2=value2
    std::size_t eq_pos;
    if((eq_pos = query_string.find('=')) != std::string::npos){
        key = UrlDecode(query_string.substr(0,eq_pos));
        value = UrlDecode(query_string.substr(eq_pos + 1));
        _get_params[key] = value;
    }
}

