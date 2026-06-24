#pragma once
#include <boost/beast/http.hpp>
#include <boost/beast.hpp>
#include <boost/asio.hpp>
#include <memory>
#include <json/json.h>
#include <json/value.h>
#include <json/reader.h>

namespace beast = boost::beast;         
namespace http = beast::http;           
namespace net = boost::asio;          
using tcp = boost::asio::ip::tcp;    

#define USERIPPREFIX  "uip_"
#define USERTOKENPREFIX  "utoken_"
#define IPCOUNTPREFIX  "ipcount_"
#define USER_BASE_INFO "ubaseinfo_"
#define LOGIN_COUNT  "logincount"
#define LOCK_COUNT "lockcount"

enum ErrorCodes {
	Success = 0,
	Error_Json = 1001,  //Json解析错误
	// RPCFailed = 1002,  //RPC请求错误
	VarifyExpired = 1003, //验证码过期
	VarifyCodeErr = 1004, //验证码错误
	UserExist = 1005,       //用户已经存在
	PasswdErr = 1006,    //密码错误
	EmailNotMatch = 1007,  //邮箱不匹配
	PasswdUpFailed = 1008,  //更新密码失败
	PasswdInvalid = 1009,   //密码更新失败
	TokenInvalid = 1010,   //Token失效
	UidInvalid = 1011,  //uid无效

	UserNotExist = 1012, //用户不存在
	EmailNotExist = 1013, //邮箱不存在
	RPCVarifyFailed = 1014, //验证的RPC请求错误
	RPCStatusFailed = 1015, //状态的RPC请求错误
};

//Defer类  raii
class Defer{
public:
    //接受一个1ambda表达式或者函数指针
    Defer(std::function<void()> func):_func(func){

    }
    //析构函数中执行传入的函数
    ~Defer(){
        _func();
    }
private:
    std::function<void()> _func;
};