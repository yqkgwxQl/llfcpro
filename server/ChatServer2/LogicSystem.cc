#include "LogicSystem.h"
#include "CSession.h"
#include "MsgNode.h"
#include <json/json.h>
#include <json/reader.h>
#include <json/value.h>
#include "RedisMgr.h"
#include "UserMgr.h"
#include "MysqlMgr.h"
#include "const.h"
#include "ChatGrpcClient.h"
#include "data.h"

LogicSystem::LogicSystem()
    :_b_stop(false){
    RejesterCallBack();
    //成员函数必须绑定到一个具体的对象上才能调用
    // _work_thread = std::thread(&DealMsg,this);  <===>
    _work_thread = std::thread([this](){
        DealMsg();
    });
}

LogicSystem::~LogicSystem(){
    _b_stop = true;
    //因为这里只有一个消费线程_worker_thread，所以用哪个都无所谓
	// _consume.notify_all();
    _cond.notify_one();
    _work_thread.join();
}

void LogicSystem::PostMsgToQue(std::shared_ptr<LogicNode> logic_node){
    std::lock_guard<std::mutex> guard_lock(_mutex);
    _logic_queues.push(logic_node);
    if(_logic_queues.size() == 1){
        _cond.notify_one();
    }
}

void LogicSystem::DealMsg(){
    for(;;){
        std::unique_lock<std::mutex> unique_lock(_mutex);
        while(_logic_queues.empty() && !_b_stop){
            _cond.wait(unique_lock);
        }
        //if确实停止了，我不能丢下逻辑队列里的数据（即要处理的请求，必须发回去）不管，必须全部处理完毕
        if(_b_stop){
            while(!_logic_queues.empty()){
                std::shared_ptr<LogicNode> logic_request_task = _logic_queues.front();
				std::cout << "recv_msg id  is " << logic_request_task->_recvNode->GetMsgId() << std::endl;
                auto iter = _callBacks.find(logic_request_task->_recvNode->GetMsgId());
                if(iter == _callBacks.end()){
                    std::cout << "the callback of id_" << logic_request_task->_recvNode->GetMsgId() << "is not found" << std::endl;
                    _logic_queues.pop();
                    continue;
                }
                iter->second(logic_request_task->_csession,std::string(logic_request_task->_recvNode->_data,logic_request_task->_recvNode->_cur_len));
                _logic_queues.pop();
                continue;
            }
            return;
        }
        //这里是正常的逻辑了
        std::shared_ptr<LogicNode> logic_request_task = _logic_queues.front();
        std::cout << "recv_msg id  is " << logic_request_task->_recvNode->GetMsgId()<< std::endl;
        auto iter = _callBacks.find(logic_request_task->_recvNode->GetMsgId());
        if(iter == _callBacks.end()){
            std::cout << "the callback of id_" << logic_request_task->_recvNode->GetMsgId() << "is not found" << std::endl;
            _logic_queues.pop();
            continue;
        }
        iter->second(logic_request_task->_csession,std::string(logic_request_task->_recvNode->_data,logic_request_task->_recvNode->_cur_len));
        //这里会多截取一个\0
        // iter->second(logic_request_task->_csession,logic_request_task->_recvNode->GetMsgId(),logic_request_task->_recvNode->_data);
        _logic_queues.pop();
    }
}

//回调函数里负责序列化数据的反序列化 ——> 业务逻辑 ——> 序列化 ——> 重新发送数据
void LogicSystem::RejesterCallBack(){
    // _callBacks[MSG_CHAT_LOGIN] = [](std::shared_ptr<CSession>,const short& msg_id,const std::string& msg_data){

    // };
    // _callBacks[MSG_CHAT_LOGIN] = std::bind(LoginHandler,this,std::placeholders::_1,std::placeholders::_2,std::placeholders::_3);
    _callBacks[MSG_CHAT_LOGIN] = std::bind(LoginHandler,this,std::placeholders::_1,std::placeholders::_2);
}

void LogicSystem::LoginHandler(std::shared_ptr<CSession> session,const std::string& msg_data){
    Json::Reader reader;
    Json::Value root;
    reader.parse(msg_data,root);
    std::cout << "user login uid is  " << root["uid"].asInt() << " user token  is "
        << root["token"].asString() << std::endl;

    Json::Value return_value;
    Defer defer([session,&return_value](){
        std::string return_string = return_value.toStyledString();
        // session->Send(msg_id,)
        session->Send(MSG_CHAT_LOGIN_RSP,return_string);
    });
    std::size_t uid = root["uid"].asInt();
    std::string uid_str = std::to_string(uid);
    std::string token = root["token"].asString();
    //从redis里面进行验证token是否正确
    std::string redis_token;
    bool success = RedisMgr::GetInstance()->Get(USERTOKENPREFIX + std::to_string(uid),redis_token);
    if(!success){
		return_value["error"] = ErrorCodes::UidInvalid;
        return;
    }

    if(redis_token != token){
		return_value["error"] = ErrorCodes::UidInvalid;
        return;
    }
    return_value["error"] = ErrorCodes::Success;

    /*std::string base_key = USER_BASE_INFO + uid_str;
    // USER_BASE_INFO = "ubaseinfo_"
    // uid_str = "1001"
    // base_key = "ubaseinfo_1001"*/
    std::string base_key = USER_BASE_INFO + uid_str;
    //准备一个空的消息结构体，用来接收从数据库/缓存查到的用户资料（姓名、昵称、头像、性别等）。
    auto user_info = std::make_shared<UserInfo>();
    bool b_base = GetBaseInfo(base_key, uid, user_info);
    if (!b_base) {
        return_value["error"] = ErrorCodes::UidInvalid;
        return;
    }
    return_value["uid"] = uid;
    return_value["pwd"] = user_info->pwd;
    return_value["name"] = user_info->name;
    return_value["email"] = user_info->email;
    return_value["nick"] = user_info->nick;
    return_value["desc"] = user_info->desc;
    return_value["sex"] = user_info->sex;
    return_value["icon"] = user_info->icon;

    //从数据库获取申请列表

    //获取好友列表

    auto server_name = ConfigMgr::GetInstance()["SelfServer"]["Name"];
    //使服务器使用数量+1
    std::string count_str = 0;
    RedisMgr::GetInstance()->Get(LOGIN_COUNT,count_str);
    std::size_t count = std::atoi(count_str.c_str());
    count++;
    count_str = std::to_string(count);
    RedisMgr::GetInstance()->Set(LOGIN_COUNT,count_str);
    //session绑定用户uid
    session->SetUserId(uid);
    //为用户设置登录ip server的名字
    std::string  ipkey = USERIPPREFIX + uid_str;
    RedisMgr::GetInstance()->Set(ipkey,server_name);
    //uid和session绑定管理,方便以后踢人操作
    UserMgr::GetInstance()->SetUserSession(uid,session);

    return;
}


bool LogicSystem::GetBaseInfo(std::string base_key, int uid, std::shared_ptr<UserInfo> &userinfo){
    //优先查redis中查询用户信息
	std::string info_str = "";
	bool b_base = RedisMgr::GetInstance()->Get(base_key, info_str);
	if (b_base) {
		Json::Reader reader;
		Json::Value root;
		reader.parse(info_str, root);
		userinfo->uid = root["uid"].asInt();
		userinfo->name = root["name"].asString();
		userinfo->pwd = root["pwd"].asString();
		userinfo->email = root["email"].asString();
		userinfo->nick = root["nick"].asString();
		userinfo->desc = root["desc"].asString();
		userinfo->sex = root["sex"].asInt();
		userinfo->icon = root["icon"].asString();
		std::cout << "user login uid is  " << userinfo->uid << " name  is "
			<< userinfo->name << " pwd is " << userinfo->pwd << " email is " << userinfo->email << std::endl;
	}
	else {
		//redis中没有则查询mysql
		//查询数据库
		std::shared_ptr<UserInfo> user_info = nullptr;
		MysqlMgr::GetInstance()->GetUser(uid,*user_info);
		if (user_info == nullptr) {
			return false;
		}

		userinfo = user_info;

		//将数据库内容写入redis缓存
		Json::Value redis_root;
		redis_root["uid"] = uid;
		redis_root["pwd"] = userinfo->pwd;
		redis_root["name"] = userinfo->name;
		redis_root["email"] = userinfo->email;
		redis_root["nick"] = userinfo->nick;
		redis_root["desc"] = userinfo->desc;
		redis_root["sex"] = userinfo->sex;
		redis_root["icon"] = userinfo->icon;
		RedisMgr::GetInstance()->Set(base_key, redis_root.toStyledString());
	}

	return true;
}
