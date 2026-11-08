#include "ChatGrpcClient.h"
#include "Logger.h"
#include "data.h"
#include "const.h"
#include "RedisMgr.h"
#include "MysqlMgr.h"

ChatConPool::ChatConPool(std::size_t size,std::string host,std::string port)
    :_size(size),_host(host),_port(port),_b_stop(false){
    for(std::size_t i = 0; i < _size; i++){
        std::shared_ptr<Channel> channel = grpc::CreateChannel(_host + ":" + _port,grpc::InsecureChannelCredentials());
        // _connections.push(std::move(StatusService::NewStub(channel)));
        _connections.push(ChatService::NewStub(channel));
    }
}

ChatConPool::~ChatConPool(){
    Close();
    std::lock_guard<std::mutex> guard_lock(_mutex);
    while (!_connections.empty()){
        _connections.pop();
    }
}

void ChatConPool::Close(){
    _b_stop = true;
    _cond.notify_all();
}

std::unique_ptr<ChatService::Stub> ChatConPool::GetConnection(){
    std::unique_lock<std::mutex> unique_lock(_mutex);
    _cond.wait(unique_lock,[this](){
        if(_b_stop)
            return true;
        return !_connections.empty();
    });
    if(_b_stop){
        return nullptr;
    }
    auto con = std::move(_connections.front());
    _connections.pop();
    return con;
}

void ChatConPool::ReturnConnection(std::unique_ptr<ChatService::Stub> con){
    {
        std::lock_guard<std::mutex> guard_lock(_mutex);
        if(_b_stop){
            return;
        }
        _connections.push(std::move(con));
    }
    _cond.notify_one();
}

ChatGrpcClient::ChatGrpcClient(){
    auto& cfg = ConfigMgr::GetInstance();
    std::vector<std::string> words;
    std::string word;
    std::string server_list = cfg["PeerServer"]["Servers"];
    std::stringstream ss(server_list);
    while(std::getline(ss,word,',')){
        words.push_back(word);
    }
    for(auto& server_name: words){
        std::string host = cfg[server_name]["Host"];
        std::string port = cfg[server_name]["Port"];
        _pools[server_name] = std::make_unique<ChatConPool>(5,host,port);
    }
}

ChatGrpcClient::~ChatGrpcClient(){
    
}

// ？？？？？？？？？？？？？这里是在外部构建req,然后在NotifyAddFriend函数内部直接使用，算是另一种写法，相较StatusGrpcClient来说
AddFriendRsp ChatGrpcClient::NotifyAddFriend(const std::string&server_name,AddFriendReq req){
    AddFriendRsp rsp;
    Defer defer([&rsp,&req](){
        rsp.set_applyuid(req.applyuid());
        rsp.set_touid(req.touid());
    });
    auto iter = _pools.find(server_name);
    if(iter == _pools.end()){
		rsp.set_error(ErrorCodes::RPCFailed);
        return rsp;
    }
    
    ClientContext context;
    ChatConPool& pool = *iter->second;
    auto stub = pool.GetConnection();
    Defer defer2([this,&pool,&stub](){
        pool.ReturnConnection(std::move(stub));
    });
    Status state = stub->NotifyAddFriend(&context,req,&rsp);
    if(state.ok()){
        rsp.set_error(ErrorCodes::Success);
        return rsp;
    }else{
		rsp.set_error(ErrorCodes::RPCFailed);
        return rsp;
    }
}

AuthFriendRsp ChatGrpcClient::NotifyAuthFriend(std::string& server_name, const AuthFriendReq& req) {
    AuthFriendRsp rsp;
    Defer defer([&rsp,&req](){
        rsp.set_fromuid(req.fromuid());
        rsp.set_touid(req.touid());
    });
    auto iter = _pools.find(server_name);
    if(iter == _pools.end()){
        rsp.set_error(ErrorCodes::RPCFailed);
        LOGE << "RPCPool not find peerserver_connection" << std::endl;
        return rsp;
    }

    ClientContext context;
    ChatConPool& pool = *iter->second;
    auto stub = pool.GetConnection();
    Defer defer2([&stub, &pool](){
        pool.ReturnConnection(std::move(stub));
    });
    auto state = stub->NotifyAuthFriend(&context, req, &rsp);
    if(state.ok()){
        rsp.set_error(ErrorCodes::Success);
    }else{
        rsp.set_error(ErrorCodes::RPCFailed);
        LOGE << "NotifyAuthFriend RPC failed: code=" << state.error_code()
         << " msg=" << state.error_message()
         << " details=" << state.error_details()
         << " server=" << server_name;
    }
    return rsp;
}

TextChatMsgRsp ChatGrpcClient::NotifyTextChatMsg(std::string server_name, 
    const TextChatMsgReq& req, const Json::Value& rtvalue) {
    LOGI << "RPC NotifyTextChatMsg -> " << server_name
     << " from=" << req.fromuid() << " to=" << req.touid()
     << " count=" << req.textmsgs_size();
    TextChatMsgRsp rsp;
    Defer defer([&rsp, &req](){
        rsp.set_fromuid(req.fromuid());
        rsp.set_touid(req.touid());
        rsp.set_thread_id(0);       //？？？？
        for(const auto& text_msg : req.textmsgs()){
            TextChatData* text_chat_data = rsp.add_textmsgs();
            text_chat_data->set_unique_id(text_msg.unique_id());
            text_chat_data->set_msg_id(text_msg.msg_id()); 
            text_chat_data->set_msgcontent(text_msg.msgcontent());
            text_chat_data->set_chat_time(text_msg.chat_time());
        }
    });
    auto iter = _pools.find(server_name);
    if(iter == _pools.end()){
        rsp.set_error(ErrorCodes::RPCFailed);
        LOGE << "RPCPool not find peerserver_connection" << std::endl;
        return rsp;
    }
    
    auto& pool = *iter->second;
    auto stub = pool.GetConnection();
    Defer defer2([ &pool, &stub](){
        pool.ReturnConnection(std::move(stub));
    });
    ClientContext context;
    Status state = stub->NotifyTextChatMsg(&context, req, &rsp);
    if(!state.ok()){
        rsp.set_error(ErrorCodes::RPCFailed);                // 只在传输层失败时覆盖
        LOGE << "NotifyTextChatMsg RPC failed: code=" << state.error_code()
             << " msg=" << state.error_message() << " server=" << server_name;
    }
    LOGI << "RPC NotifyTextChatMsg done, state.ok=" << state.ok()
     << " rsp.error=" << rsp.error();
    return rsp;
}

//其实我觉得这里根本没被用到？？？？？？？？？？？？？？？？？？？？？？
bool ChatGrpcClient::GetBaseInfo(std::string base_key, int uid, std::shared_ptr<UserInfo>& userinfo) {
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
		UserInfo user_info;
		bool is_get = MysqlMgr::GetInstance()->GetUser(uid,user_info);
		if (!is_get) {
			return false;
		}

		*userinfo = user_info;

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
