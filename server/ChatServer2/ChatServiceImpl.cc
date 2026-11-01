#include "ChatServiceImpl.h"
#include "Logger.h"
#include "data.h"
#include "CServer.h"
#include "UserMgr.h"
#include "CSession.h"
#include "RedisMgr.h"
#include "MysqlMgr.h"

// ChatServiceImpl::ChatServiceImpl(std::shared_ptr<CServer> server):_p_server(server){

// }
ChatServiceImpl::ChatServiceImpl(){

}

void ChatServiceImpl::SetServer(std::shared_ptr<CServer> server){
    _p_server = server;
}

//这里reply并不起主要作用，因为这是ChatServer1和ChatServer2之间的通信，这里的request和reply只是满足ChatServer1和ChatServer2的需求
//而实际上业务则主要是用户1的Client1和用户2的Client2之间的交互（ChatServer1和ChatServer2只是所通过的手段）
Status ChatServiceImpl::NotifyAddFriend(ServerContext* context, const AddFriendReq* request,
    AddFriendRsp* response) {
    //查看
    Defer defer([request,response](){
        response->set_applyuid(request->applyuid());
        response->set_touid(request->touid());
    });
    Json::Value return_value;
    return_value["applyuid"] = request->applyuid();
    return_value["name"] = request->name();
    return_value["desc"] = request->desc();
    return_value["icon"] = request->icon();
    return_value["nick"] = request->nick();
    return_value["sex"] = request->sex();
    return_value["touid"] = request->touid();
    return_value["error"] = ErrorCodes::Success;

    std::string return_string = return_value.toStyledString();
    auto peer_session = UserMgr::GetInstance()->GetSession(request->touid());
    if(!peer_session){
        response->set_error(ErrorCodes::RPCFailed);
        return Status::OK;
    }
    response->set_error(ErrorCodes::Success);
    peer_session->Send(ID_NOTIFY_ADD_FRIEND_REQ,return_string);

    return Status::OK;
}

Status ChatServiceImpl::NotifyAuthFriend(ServerContext* context,
    const AuthFriendReq* request, AuthFriendRsp* response) {
    Defer defer([response,request](){
        response->set_fromuid(request->fromuid());
        response->set_touid(request->touid());
    });
    int uid = request->fromuid();
    int toUid = request->touid();
    std::shared_ptr<UserInfo> self_user_info;
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
        response->set_error(ErrorCodes::RPCFailed);
        return Status::OK;
    }
    response->set_error(ErrorCodes::Success);
    std::string notify_string = notify.toStyledString();
    peer_session->Send(ID_NOTIFY_AUTH_FRIEND_REQ, notify_string);
    return Status::OK;
}

Status ChatServiceImpl::NotifyTextChatMsg(::grpc::ServerContext* context,
    const TextChatMsgReq* request, TextChatMsgRsp* response) {
    LOGI << "RPC NotifyTextChatMsg 收到: from=" << request->fromuid()
     << " to=" << request->touid() << " count=" << request->textmsgs_size();
    int self_uid = request->touid();
    Json::Value notify;
    notify["error"] = ErrorCodes::Success;
    notify["from_uid"] = request->fromuid();
    notify["to_uid"] = request->touid();
    Json::Value text_array;
    for(const auto& text_msg : request->textmsgs()){
        Json::Value obj;
        obj["unique_id"] = text_msg.unique_id();
        obj["msg_id"] = text_msg.msg_id();
        obj["content"] = text_msg.msgcontent();
        obj["chat_time"] = text_msg.chat_time();
        text_array.append(obj);
    }
    notify["text_array"] = text_array;
    std::string notify_str = notify.toStyledString();
    auto peer_session = UserMgr::GetInstance()->GetSession(self_uid);
    if(!peer_session){
        LOGE << "使用RPC，未能在远端服务器中找到to_uid(" << self_uid << ")对应的session";
        response->set_error(ErrorCodes::RPCFailed);
        return Status::OK;    // ★ 必须 return；传输层仍报 OK，业务错误码放 error 里
    }
    peer_session->Send(ID_NOTIFY_TEXT_CHAT_MSG_REQ, notify_str);
    return Status::OK;
}

bool ChatServiceImpl::GetBaseInfo(std::string base_key, int uid, std::shared_ptr<UserInfo>& userinfo) {
    if(!userinfo) 
        userinfo = std::make_shared<UserInfo>();
    std::string info_str = "";
    bool is_get = RedisMgr::GetInstance()->Get(base_key,info_str);
    if(is_get){
        Json::Reader reader;
        Json::Value redis_root;
        reader.parse(info_str, redis_root);
        userinfo->uid = redis_root["uid"].asInt();
        userinfo->pwd = redis_root["pwd"].asString();
        userinfo->name = redis_root["name"].asString();
        userinfo->email = redis_root["email"].asString();
        userinfo->nick = redis_root["nick"].asString();
        userinfo->desc = redis_root["desc"].asString();
        userinfo->sex = redis_root["sex"].asInt();
        userinfo->icon = redis_root["icon"].asString();
    }else{
        std::shared_ptr<UserInfo> user_info = std::make_shared<UserInfo>();
        if(!MysqlMgr::GetInstance()->GetUser(uid,*user_info)){
            LOGE << "user uid invalid " << std::endl;
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
        RedisMgr::GetInstance()->Set(base_key,redis_root.toStyledString());
    }
    return true;
}