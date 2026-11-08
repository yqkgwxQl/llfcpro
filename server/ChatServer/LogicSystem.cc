#include "LogicSystem.h"
#include "Logger.h"
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
#include "message.grpc.pb.h"

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
				LOGI << "recv_msg id  is " << logic_request_task->_recvNode->GetMsgId();
                auto iter = _callBacks.find(logic_request_task->_recvNode->GetMsgId());
                if(iter == _callBacks.end()){
                    LOGI << "the callback of id_" << logic_request_task->_recvNode->GetMsgId() << "is not found";
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
        LOGI << "recv_msg id  is " << logic_request_task->_recvNode->GetMsgId()<< std::endl;
        auto iter = _callBacks.find(logic_request_task->_recvNode->GetMsgId());
        if(iter == _callBacks.end()){
            LOGI << "the callback of id_" << logic_request_task->_recvNode->GetMsgId() << " is not found" << std::endl;
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
    _callBacks[ID_SEARCH_USER_REQ] = std::bind(SearchUserHandler,this,std::placeholders::_1,std::placeholders::_2);
    _callBacks[ID_ADD_FRIEND_REQ] = std::bind(AddFriendApply,this,std::placeholders::_1,std::placeholders::_2);
    _callBacks[ID_AUTH_FRIEND_REQ] = std::bind(AuthFriendApply,this,std::placeholders::_1,std::placeholders::_2);
    _callBacks[ID_GET_APPLY_LIST_REQ] = std::bind(GetApplyListHandler,this,std::placeholders::_1,std::placeholders::_2);
    _callBacks[ID_GET_FRIEND_LIST_REQ] = std::bind(GetFriendListHandler,this,std::placeholders::_1,std::placeholders::_2);
    _callBacks[ID_TEXT_CHAT_MSG_REQ] = std::bind(DealChatTextMsg,this,std::placeholders::_1,std::placeholders::_2);
}

void LogicSystem::LoginHandler(std::shared_ptr<CSession> session,const std::string& msg_data){
    Json::Reader reader;
    Json::Value root;
    reader.parse(msg_data,root);
    LOGI << "user login uid is  " << root["uid"].asInt() << " user token  is "
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
    //由于这里直接redis本地校验，因此不会使用StatusGrpcClient,StatusGrpcClient被搁置不会使用
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

        //这里不在登录时就获取申请列表，改为登陆后，客户端主动发送申请申请列表的请求，然后服务端给出响应即可
    // //从数据库获取申请列表
    // std::vector<std::shared_ptr<ApplyInfo>> applyList;
    // bool is_get = GetFriendApplyInfoList(uid,applyList);
    // if(!is_get){
    //     LOGE << "failed get friend_applyinfo_list ";
    //     return;
    // }
    // for(auto& applyinfo : applyList){
    //     Json::Value obj;
    //     obj["uid"] = applyinfo->uid_;
    //     obj["name"] = applyinfo->name_;
    //     obj["desc"] = applyinfo->desc_;
    //     obj["icon"] = applyinfo->icon_;
    //     obj["nick"] = applyinfo->nick_;
    //     obj["sex"] = applyinfo->sex_;
    //     obj["status"] = applyinfo->status_;
    //     return_value["apply_list"].append(obj);
    // }

    //从数据库获取好友列表

    /*最终的json:
        {
            "error": 0,
            "uid": 1001,
            "apply_list": [
                { "name": "张三", "uid": 1002, "icon": "...", "nick": "...", "sex": 1, "desc": "...", "status": 0 },
                { "name": "李四", "uid": 1003, ... }
            ]
        }*/
    //获取好友列表

    auto server_name = ConfigMgr::GetInstance()["SelfServer"]["Name"];
    //使服务器使用数量+1
    //这里写多线程可能出问题
    // std::string count_str = "";
    // count_str = RedisMgr::GetInstance()->HGet(LOGIN_COUNT,server_name);
    // std::size_t count = std::atoi(count_str.c_str());
    // count++;
    // RedisMgr::GetInstance()->HSet(LOGIN_COUNT,server_name,std::to_string(count));
    RedisMgr::GetInstance()->HIncrBy(LOGIN_COUNT, server_name, 1);
    //session绑定用户uid
    session->SetUserId(uid);
    //为用户设置登录ip server的名字
    std::string  ipkey = USERIPPREFIX + uid_str;
    RedisMgr::GetInstance()->Set(ipkey,server_name);        //？？？？？？？？？？？放心，这里用户掉线时，绝对要删的
    //uid和session绑定管理,方便以后踢人操作
    UserMgr::GetInstance()->SetUserSession(uid,session);

    return;
}

void LogicSystem::SearchUserHandler(std::shared_ptr<CSession> session,const std::string& msg_data){
    Json::Reader reader;
    Json::Value root;
    reader.parse(msg_data,root);
    std::string search_uid_str = root["search_uid"].asString();
    LOGI << "search_uid is " << search_uid_str << std::endl;

    Json::Value return_value;
    Defer defer([session,&return_value](){
        std::string return_string = return_value.toStyledString();
        session->Send(ID_SEARCH_USER_RSP,return_string);
    });

    std::string base_key = USER_BASE_INFO + search_uid_str;
    // int search_uid = std::stoi(search_uid_str);
    //stoi这里可能失败，一旦失败，如果没有捕获异常，则整个程序崩溃
    int search_uid = 0;
    try{
        search_uid = std::stoi(search_uid_str);
    }catch(const std::invalid_argument& e){
        LOGE << "search_uid(" << search_uid_str << ")" << "此uid无法转换为数字";
        return_value["error"] = ErrorCodes::UidInvalid;
        return;
    }catch(const std::out_of_range& e2){
        LOGE << "search_uid(" << search_uid_str << ")" << " 数值超出int范围";
        return_value["error"] = ErrorCodes::UidInvalid;
        return;
    }
    //std::stoi : string -> int
    //std::atoi : const char* -> int
    auto search_user_info = std::make_shared<UserInfo>();
    bool is_search = GetBaseInfo(base_key,search_uid,search_user_info);
    if (!is_search) {
        return_value["error"] = ErrorCodes::UidInvalid;
        return;
    }
    return_value["uid"] = search_uid;
    return_value["pwd"] = search_user_info->pwd;
    return_value["name"] = search_user_info->name;
    return_value["email"] = search_user_info->email;
    return_value["nick"] = search_user_info->nick;
    return_value["desc"] = search_user_info->desc;
    return_value["sex"] = search_user_info->sex;
    return_value["icon"] = search_user_info->icon;

    //下面要获取搜索者和被搜索人的关系
    int relation = -1;
    int self_uid = static_cast<int>(session->GetUserId());
    if(self_uid == 0){                              // 没登录就搜
        return_value["error"] = ErrorCodes::TokenInvalid;
        return;
    }
    if(!MysqlMgr::GetInstance()->GetRelation(self_uid, search_uid, relation)){
        LOGE << "GetRelation failed, self_uid=" << self_uid << " other_uid=" << search_uid;
        return_value["error"] = ErrorCodes::MysqlMgr_GetRelation_error;
        return;
    }
    return_value["relation"] = relation;
    return_value["error"] = ErrorCodes::Success;

    return;
}

void LogicSystem::GetApplyListHandler(std::shared_ptr<CSession> session, const std::string& msg_data){
    Json::Reader reader;
    Json::Value root;
    reader.parse(msg_data,root);

    const int uid = root["uid"].asInt();    
    const long long before = root.get("begin", 0).asInt64();    //如果有root["begin"]字段的话，返回该字段。否则返回default_value, 即0
    int limit = root.get("limit", 10).asInt();
    LOGI << "uid is : " << uid << " before_id is : " << before << " limit is : " << limit;
    
    //关键：服务端必须限幅，否则客户端传 1000 会把包撑爆（MAX_LENGTH=2048）
    if(limit <= 0 || limit > 10) 
        limit = 10;
    
    std::vector<std::shared_ptr<ApplyInfo>> applyList;
    bool is_get = MysqlMgr::GetInstance()->GetApplyList(uid, applyList, before, limit);
    Json::Value return_value;
    if(!is_get){
        return_value["error"] = ErrorCodes::MysqlMgr_GetApplyList_error;
        LOGE << "MysqlMgr::GetInstance()->GetApplyList(uid, applyList, before, limit) error !";
    }else{
        return_value["error"] = ErrorCodes::Success;
    }
    Defer defer([session,&return_value](){
        std::string return_string = return_value.toStyledString();
        session->Send(ID_GET_APPLY_LIST_RSP, return_string);
    });
    for(const auto& apply_info : applyList){
        Json::Value obj;
        obj["uid"] = apply_info->uid_;
        obj["name"] = apply_info->name_;
        obj["desc"] = apply_info->desc_;
        obj["icon"] = apply_info->icon_;
        obj["nick"] = apply_info->nick_;
        obj["sex"] = apply_info->sex_;
        obj["status"] = apply_info->status_;
        obj["id"] = apply_info->id_;
        return_value["apply_list"].append(obj);
    }
    return_value["has_more"] = (static_cast<int>(applyList.size()) == limit);
    
}

void LogicSystem::GetFriendListHandler(std::shared_ptr<CSession> session, const std::string& msg_data){
    Json::Reader reader;
    Json::Value root;
    reader.parse(msg_data,root);

    const int uid = root["uid"].asInt();    
    const long long before = root.get("begin", 0).asInt64();    //如果有root["begin"]字段的话，返回该字段。否则返回default_value, 即0
    int limit = root.get("limit", 10).asInt();
    LOGI << "uid is : " << uid << " before_id is : " << before << " limit is : " << limit;
    
    //关键：服务端必须限幅，否则客户端传 1000 会把包撑爆（MAX_LENGTH=2048）
    if(limit <= 0 || limit > 10) 
        limit = 10;
    
    std::vector<std::shared_ptr<FriendInfo>> friendList;
    bool is_get = MysqlMgr::GetInstance()->GetFriendList(uid, friendList, before, limit);
    Json::Value return_value;
    if(!is_get){
        return_value["error"] = ErrorCodes::MysqlMgr_GetFriendList_error;
        LOGE << "MysqlMgr::GetInstance()->GetFriendList(uid, friendList, before, limit) error !";
    }else
        return_value["error"] = ErrorCodes::Success;
    Defer defer([session,&return_value](){
        std::string return_string = return_value.toStyledString();
        session->Send(ID_GET_FRIEND_LIST_RSP, return_string);
    });
    for(const auto& friend_info : friendList){
        Json::Value obj;
        obj["uid"] = friend_info->uid_;
        obj["name"] = friend_info->name_;
        obj["desc"] = friend_info->desc_;
        obj["icon"] = friend_info->icon_;
        obj["nick"] = friend_info->nick_;
        obj["sex"] = friend_info->sex_;
        obj["id"] = friend_info->id_;
        return_value["friend_list"].append(obj);
    }
    return_value["has_more"] = (static_cast<int>(friendList.size()) == limit);
}
/* 
        ClientA 发送 ID_ADD_FRIEND_REQ 给 ChatServer1,ChatServer1发送 ID_ADD_FRIEND_RSP 作为申请好友的回复给 ClientA;  同时你必须发给
            对端服务器，或者说实际上最终是对端客户端ClientB
   satuation1: 对端客户端ClientB和ClientA在同一个聊天服务器ChatServer1,那只需要找到ChatServer1下ClientB的session,
        session->Send(ID_NOTIFY_ADD_FRIEND_REQ,return_string)即可, 然后ClientB就收到ID_NOTIFY_ADD_FRIEND_REQ回传了
   satuation2: 对端客户端ClientB在ChatServer2,ClientA在ChatServer1.然后通过rpc ——> ChatServer1发送AddFriendReq向ChatServer2,
   ChatServer2返回AddFriendReq通过grpc回ChatServer1),并且ChatServer2调用它的session,
   session->Send(ID_NOTIFY_ADD_FRIEND_REQ,return_string), 然后ClientB就收到ID_NOTIFY_ADD_FRIEND_REQ回传了*/
void LogicSystem::AddFriendApply(std::shared_ptr<CSession> session,const std::string& msg_data){
    Json::Value root;
    Json::Reader reader;
    reader.parse(msg_data,root);
    auto uid = root["uid"].asInt();
    auto desc = root["desc"].asString();
    auto bakname = root["bakname"].asString();
    auto to_uid = root["touid"].asInt();
    LOGI << " 客户端申请好友请求:" << " client_uid is " << uid << " applydesc  is " <<
        desc << " the backname of applyer_to_be applyed er is " << bakname << " be applyed er _uid is" << to_uid;
    Json::Value return_value;
    Defer defer([session,&return_value](){
        std::string return_string = return_value.toStyledString();
        session->Send(ID_ADD_FRIEND_RSP,return_string);
    });
    return_value["error"] = ErrorCodes::Success;

    //存申请关系到数据库中
    MysqlMgr::GetInstance()->AddFriendApply(uid,to_uid);

    //接下来判断是否ClientB是位于ChatServer1的session下
    std::string ip_key = USERIPPREFIX + std::to_string(to_uid);
    std::string peer_server_name("");
    bool is_ip = RedisMgr::GetInstance()->Get(ip_key,peer_server_name);
    //对方不在线，直接返回
    if(!is_ip){
        LOGI << std::to_string(to_uid) + "对方不在线，直接返回添加好友通知" << std::endl;
        return;
    }
    auto& cfg = ConfigMgr::GetInstance();
    std::string self_server_name = cfg["SelfServer"]["Name"];
    std::string base_key = USER_BASE_INFO + std::to_string(uid);
    auto user_info_applyer = std::make_shared<UserInfo>();
    bool is_get_baseinfo = GetBaseInfo(base_key,uid,user_info_applyer);

    if(self_server_name == peer_server_name){
        Json::Value notify;
        notify["error"] = ErrorCodes::Success;
        notify["applyuid"] = uid;
        notify["desc"] = desc;
        notify["touid"] = to_uid;
        if(is_get_baseinfo){
            notify["name"] = user_info_applyer->name;
            notify["icon"] = user_info_applyer->icon;
            notify["sex"] = user_info_applyer->sex;
            notify["nick"] = user_info_applyer->nick;
        }
        auto peer_session = UserMgr::GetInstance()->GetSession(to_uid);
        if(!peer_session){
            LOGE << "to_uid(" << to_uid << ") 在 Redis 记录为在线，但本机没有 session（脏 uip_？）";
            return;
        }
        peer_session->Send(ID_NOTIFY_ADD_FRIEND_REQ,notify.toStyledString());
    }else{
        //ClientB不和ClientA在一个ChatServer，因此通过grpc提醒ClientB
        AddFriendReq notify_friend_apply_req;
        notify_friend_apply_req.set_applyuid(uid);
        notify_friend_apply_req.set_desc(desc);
        notify_friend_apply_req.set_touid(to_uid);
        if(is_get_baseinfo){
            notify_friend_apply_req.set_name(user_info_applyer->name);
            notify_friend_apply_req.set_nick(user_info_applyer->nick);
            notify_friend_apply_req.set_sex(user_info_applyer->sex);
            notify_friend_apply_req.set_icon(user_info_applyer->icon);
        }
        AddFriendRsp notify_friend_apply_rsp = ChatGrpcClient::GetInstance()->NotifyAddFriend(peer_server_name,notify_friend_apply_req);
        if(notify_friend_apply_rsp.error()){
            LOGI << "notify_friend_apply_rsp failed";
        }
    }
}

void LogicSystem::AuthFriendApply(std::shared_ptr<CSession> session,const std::string& msg_data){
    Json::Value root;
    Json::Reader reader;
    reader.parse(msg_data,root);
    int uid = root["uid"].asInt();      // 同意申请的同意者的uid
    std::string desc = root["desc"].asString();
    std::string bakname = root["bakname"].asString();
    int toUid = root["touid"].asInt();  //发送申请给被申请者的给被申请者(这时已经同意了)的发送者的uid
    LOGI << "客户端发送认证好友申请的请求到自己的服务器上, " << "auth_friend_apply_req-------" << " 同意申请的同意者的uid---uid is " 
    << uid << " 发送申请给被申请者的发送者的uid---to_uid is " << toUid << " 自己给对方自己的描述 desc is " << desc << 
    " 自己给对方起的备注名 bakname is " << bakname << std::endl;
    //？？？？？？？？？这段描述不太好
    Json::Value return_value;
    std::shared_ptr<UserInfo> userInfo = std::make_shared<UserInfo>();
    std::string baseKey = USER_BASE_INFO + std::to_string(toUid);
    bool is_getBaseInfo = GetBaseInfo(baseKey,toUid,userInfo);
    if(!is_getBaseInfo){
        LOGI << "GetBaseInfo error, explain uid error!" << std::endl;
        return_value["error"] = ErrorCodes::UidInvalid;
    }else{
        return_value["error"] = ErrorCodes::Success;
        return_value["touid"] = toUid;
        return_value["name"] = userInfo->name;
        return_value["nick"] = userInfo->nick;
        return_value["icon"] = userInfo->icon;
        return_value["sex"] = userInfo->sex;
    }

    Defer defer([session,&return_value](){
        std::string return_string = return_value.toStyledString();
        session->Send(ID_AUTH_FRIEND_RSP,return_string);
    });

    //更新数据库的表: 1.friend_apply表中的status->表示是否通过了好友认证 2.添加好友 -> 在friend表中添加自己和peer各自的好友信息
    // MysqlMgr::GetInstance()->AuthFriendApply(uid,toUid);
    MysqlMgr::GetInstance()->AuthFriendApply(toUid,uid);
    MysqlMgr::GetInstance()->AddFriend(uid,toUid,bakname);

    std::string ipKey = USERIPPREFIX + std::to_string(toUid);
    std::string peerServerName("");
    bool is_ip = RedisMgr::GetInstance()->Get(ipKey,peerServerName);
    if(!is_ip)
        return;
    auto& cfg = ConfigMgr::GetInstance();
    std::string selfServerName = cfg["SelfServer"]["Name"];
    if(selfServerName == peerServerName){
        std::shared_ptr<UserInfo> self_user_info = std::make_shared<UserInfo>();
        std::string self_user_base_key = USER_BASE_INFO + std::to_string(uid);
        bool is_get = GetBaseInfo(self_user_base_key, uid, self_user_info);
        Json::Value notify;
        if(is_get){
            notify["error"] = ErrorCodes::Success;
            notify["peer_uid"] = uid;
            notify["peer_name"] = self_user_info->name;
            notify["peer_nick"] = self_user_info->nick;
            notify["peer_icon"] = self_user_info->icon;
            notify["peer_sex"] = self_user_info->sex;
        }else{
            notify["error"] = ErrorCodes::UidInvalid;
        }
        std::shared_ptr<CSession> peer_session = UserMgr::GetInstance()->GetSession(toUid);
        if(!peer_session){
            LOGE << "to_uid(" << toUid << ") 在 Redis 记录为在线，但本机没有 session（脏 uip_？）";
            return;
        }

        std::string notify_string = notify.toStyledString();
        peer_session->Send(ID_NOTIFY_AUTH_FRIEND_REQ, notify_string);
    }else{
        AuthFriendReq req;
        req.set_fromuid(uid);
        req.set_touid(toUid);
        AuthFriendRsp rsp = ChatGrpcClient::GetInstance()->NotifyAuthFriend(peerServerName,req);
        if(rsp.error()){
            LOGI << "AuthFriendRsp_rsp failed, error_code: " <<  rsp.error();
        }
    }
}

void LogicSystem::DealChatTextMsg(std::shared_ptr<CSession> session, const std::string& msg_data){
    Json::Value root;
    Json::Reader reader;
    reader.parse(msg_data, root);
    // 发送者以 session 绑定的 uid 为准（客户端传的 from_uid 可伪造）
    const int from_uid = static_cast<int>(session->GetUserId());
    // const int from_uid = root["from_uid"].asInt();
    const int to_uid = root["to_uid"].asInt();
    const Json::Value& text_array = root["text_array"];
    LOGI << "用户A(uid:" << from_uid << ")发送text_msg给用户B(uid:" << to_uid << ")";
    
    Json::Value return_value;
    return_value["error"] = ErrorCodes::Success;
    if(from_uid <= 0 || to_uid <= 0 || from_uid == to_uid
       || !text_array.isArray() || text_array.size() == 0){
        LOGE << "DealChatTextMsg 参数非法: from_uid=" << from_uid
             << " to_uid=" << to_uid << " text_size=" << text_array.size();
        return_value["error"] = ErrorCodes::Error_Json;
        session->Send(ID_TEXT_CHAT_MSG_RSP, return_value.toStyledString());
        return;
    }
    return_value["from_uid"] = from_uid;
    return_value["to_uid"] = to_uid;
    for(const auto& text_obj : text_array){
        if(text_obj.isObject())
            return_value["unique_ids"].append(text_obj["unique_id"].asString());
    }
    Defer defer([&return_value, session](){
        std::string return_string = return_value.toStyledString();
        session->Send(ID_TEXT_CHAT_MSG_RSP, return_string);
    });
    
    std::string ipKey = USERIPPREFIX + std::to_string(to_uid);
    std::string peer_server_name = "";
    bool is_ip = RedisMgr::GetInstance()->Get(ipKey, peer_server_name);
    if(!is_ip){
        LOGI << std::to_string(to_uid) + "对方不在线，暂时将发送的信息存储到数据库中" << std::endl;     //？？？？？？以后需要进行修改
        return_value["online"] = false;
        return;
    }
    auto& config = ConfigMgr::GetInstance();
    std::string self_server_name = config["SelfServer"]["Name"];
    if(peer_server_name == self_server_name){
        auto peer_session = UserMgr::GetInstance()->GetSession(to_uid);
        if(peer_session){
            Json::Value notify;
            notify["error"] = ErrorCodes::Success;
            notify["from_uid"] = from_uid;
            notify["to_uid"] = to_uid;
            notify["text_array"] = text_array;
            return_value["onlien"] = true;

            std::string notify_str = notify.toStyledString();
            peer_session->Send(ID_NOTIFY_TEXT_CHAT_MSG_REQ, notify_str);
        }else{
            LOGE << "未找到to_uid(" << to_uid << ")对应的session";
            return_value["online"] = false;
        }
    }else{
        TextChatMsgReq text_chat_msg_req;
        text_chat_msg_req.set_fromuid(from_uid);
        text_chat_msg_req.set_touid(to_uid);
        text_chat_msg_req.set_thread_id(0);     //？？？？ 目前暂填0
        for(const auto& text_obj : text_array){
            if(!text_obj.isObject())
                continue;
            TextChatData* text_chat_data = text_chat_msg_req.add_textmsgs();
            text_chat_data->set_chat_time(text_obj["chat_time"].asString());
            text_chat_data->set_unique_id(text_obj["unique_id"].asString());
            text_chat_data->set_msgcontent(text_obj["content"].asString());
            text_chat_data->set_msg_id(0);     //？？？？ 目前暂填0
        }                                                                                           //？？？我感觉rtvalue没有任何作用
        TextChatMsgRsp text_chat_msg_rsp = ChatGrpcClient::GetInstance()->NotifyTextChatMsg(peer_server_name, text_chat_msg_req, return_value);
        if(text_chat_msg_rsp.error()){
            LOGI << "NotifyTextChatMsg failed, error_code: " <<  text_chat_msg_rsp.error();
            return_value["error"] = ErrorCodes::RPCFailed;
            return_value["online"] = false;             
        }else{
            return_value["online"] = true;
        }
    }
}

//感觉这里也可以写成bool LogicSystem::GetBaseInfo(std::string& base_key,std::shared_ptr<UserInfo> &userinfo)？？？？？？？？？？？？？？？
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
		LOGI << "user login uid is  " << userinfo->uid << " name  is "
			<< userinfo->name << " pwd is " << userinfo->pwd << " email is " << userinfo->email << std::endl;
	}
	else {
		//redis中没有则查询mysql
		//查询数据库
		auto user_info = std::make_shared<UserInfo>();
		if (!MysqlMgr::GetInstance()->GetUser(uid, *user_info)) {
            LOGE << "redis和mysql中均未查到uid :" << uid << " 对应的数据";
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


// bool LogicSystem::GetFriendApplyInfoList(int uid,std::vector<std::shared_ptr<ApplyInfo>>& list){
//     return MysqlMgr::GetInstance()->GetApplyList(uid,list,0,10);
// }

// bool LogicSystem::GetHasFriendInfoList(int uid,std::vector<std::shared_ptr<FriendInfo>>& list){
//     return MysqlMgr::GetInstance()->GetFriendList(uid,list,0,10);
// }