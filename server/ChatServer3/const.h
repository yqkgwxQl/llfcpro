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

#define CODEPREFIX  "code_"
#define USERTOKENPREFIX  "utoken_"
#define LOGIN_COUNT  "logincount"
#define USER_BASE_INFO "ubaseinfo_"
#define USERIPPREFIX  "uip_"

enum ErrorCodes {
	Success = 0,
	Error_Json = 1001,  //Json解析错误

	RPCFailed = 1002,  //RPC请求错误
	
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
	MysqlMgr_GetUser_error = 1016,// MysqlMgr::GetInstance()->GetUser()错误
};

enum MSG_IDS {
	MSG_CHAT_LOGIN = 1005, //用户登陆
	MSG_CHAT_LOGIN_RSP = 1006, //用户登陆回包
};

#define MAX_LENGTH  1024*2
//头部总长度
#define HEAD_TOTAL_LEN 4
//头部id长度
#define HEAD_ID_LEN 2
//头部数据长度
#define HEAD_DATA_LEN 2
#define MAX_RECVQUE  10000
#define MAX_SENDQUE 1000

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