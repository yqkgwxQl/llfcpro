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
	
	MysqlMgr_GetUser_error = 2016,// MysqlMgr::GetInstance()->GetUser()错误
	MysqlMgr_GetApplyList_error = 2017,	//MysqlMgr::GetInstance()->GetApplyList()错误
	MysqlMgr_GetFriendList_error = 2018,//MysqlMgr::GetInstance()->GetFriendList()错误
	MysqlMgr_GetRelation_error = 2019,//MysqlMgr::GetInstance()->GetRelation()错误
};

enum MSG_IDS {
	MSG_CHAT_LOGIN = 1005, //用户登陆
	MSG_CHAT_LOGIN_RSP = 1006, //用户登陆回包
	ID_SEARCH_USER_REQ = 1007, //用户搜索请求
	ID_SEARCH_USER_RSP = 1008, //搜索用户回包
	ID_ADD_FRIEND_REQ = 1009, //申请添加好友请求
    ID_ADD_FRIEND_RSP = 1010,  //添加好友申请的回复
    ID_NOTIFY_ADD_FRIEND_REQ = 1011,
    // 注意 1009,1010,1011号信息 eg: ClienA发送给ChatServer1 1009, ChatServer1处理后返回 1010给ClientA, 同时ChatServer1通过grpc拿到ChatServer2，发送给ClientB
	ID_AUTH_FRIEND_REQ = 1013,		//认证好友请求
	ID_AUTH_FRIEND_RSP = 1014,		//认证好友回复
	ID_NOTIFY_AUTH_FRIEND_REQ = 1015,	//通知认证好友申请
	ID_TEXT_CHAT_MSG_REQ  = 1017,  //文本聊天信息请求
    ID_TEXT_CHAT_MSG_RSP  = 1018,  //文本聊天信息回复
    ID_NOTIFY_TEXT_CHAT_MSG_REQ = 1019, //通知用户文本聊天信息
	//------mine---------	
	//我为什么在用户登录时要直反给他10条apply,既然一条消息包含不了那么多apply，为什么我们不多发几条呢? ————> 见addition.txt
	ID_GET_APPLY_LIST_REQ = 1040,	//用户刚登陆时只返回给他10条 别人想要添加他为好友的申请（因为一条信息是有限大小的，先第一条回复里
									//包含10个apply, 后续用户如果在他的客户端里向下翻动它的申请列表，则需要发送此请求来获取后续apply)
	ID_GET_APPLY_LIST_RSP = 1041,
	ID_GET_FRIEND_LIST_REQ = 1042,	//同ID_GET_APPLY_LIST_REQ
	ID_GET_FRIEND_LIST_RSP = 1043,
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