#include "StatusServiceImpl.h"
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/random_generator.hpp>
#include <boost/uuid/uuid_io.hpp>
#include "ConfigMgr.h"
#include "const.h"
#include "RedisMgr.h"

ChatServer::ChatServer()
    :_host(""),_port(""),_name(""),_con_count(0){

};

ChatServer::ChatServer(const std::string& host,const std::string& port,const std::string& name)
    :_host(host),_port(port),_name(name),_con_count(0){

}

ChatServer::~ChatServer(){

}

ChatServer::ChatServer(const ChatServer& cs){
    _host = cs._host;
    _port = cs._port;
    _name = cs._name;
    //con_count 的变化应该由实际业务事件驱动，而不是由 C++ 对象的构造/拷贝驱动： 这里只需要简单的拷贝赋值和拷贝构造即可
    // cs._con_count +=1;
    // _con_count = cs._con_count;
    _con_count = cs._con_count;
}

ChatServer& ChatServer::operator =(const ChatServer& cs){
    if(this == &cs){
        return *this;
    }
    _host = cs._host;
    _port = cs._port;
    _name = cs._name;
    _con_count = cs._con_count;
    return *this;
}

std::string generate_unique_string(){
    //创建uuid对象
    boost::uuids::uuid uuid= boost::uuids::random_generator()();
    std::string uuid_string = boost::uuids::to_string(uuid);
    return uuid_string;
}

StatusServiceImpl::StatusServiceImpl(){
    auto& cfgMgr = ConfigMgr::GetInstance();
    //-----------
    std::string chatServers = cfgMgr["chatservers"]["Name"];
    std::vector<std::string> words;
    std::string word;
    std::stringstream ss(chatServers);
    while(std::getline(ss,word,',')){
        words.emplace_back(word);
    }
    //-----------
    for(auto& word_2: words){
        if (cfgMgr[word_2]["Name"].empty()) {
			continue;
		}
        std::string name = cfgMgr[word_2]["Name"];
        std::string host = cfgMgr[word_2]["Host"];
        std::string port = cfgMgr[word_2]["Port"];
        auto chatServer = ChatServer(host,port,name);
        _servers.insert(std::pair(name,chatServer));
    }
}

StatusServiceImpl::~StatusServiceImpl(){

}

Status StatusServiceImpl::GetChatServer(::grpc::ServerContext* context, const ::message::GetChatServerReq* request, 
    ::message::GetChatServerRsp* response){
    // std::string prefix("llfc status server has received :  ");
    const ChatServer& server = getChatServer();
    response->set_error(ErrorCodes::Success);
    response->set_host(server._host);
    response->set_port(server._port);
    response->set_token(generate_unique_string());
    insertToken(request->uid(),response->token());
    return Status::OK;
}

::grpc::Status StatusServiceImpl::Login(::grpc::ClientContext* context, const ::message::LoginReq& request,
         ::message::LoginRsp* response){
    int uid = request.uid();
    std::string token = request.token();

    std::string uid_str = std::to_string(uid);
    std::string token_key = USERTOKENPREFIX + uid_str;
    std::string redis_token = "";
    //从redis中查询token_key对应的token
    bool success = RedisMgr::GetInstance()->Get(token_key,redis_token);
    if(!success){
        response->set_error(ErrorCodes::UidInvalid);  //←应用层：但业务逻辑上uid 无效
        return Status::OK; //<传输层：请求送达了，服务器正常运行
    }
    if(redis_token != token){
        response->set_error(ErrorCodes::TokenInvalid);
        return Status::OK;
    }
    response->set_error(ErrorCodes::Success);
    response->set_uid(uid);
    response->set_token(token);
    return Status::OK;
}

/*atoi vs stoi 区别
对比	atoi (C 风格)	stoi (C++ 风格)
头文件	<cstdlib> 或 <stdlib.h>	<string>
参数类型	const char*	std::string
错误处理	无法检测错误，转换失败返回 0	抛出异常：无效输入抛 std::invalid_argument，越界抛 std::out_of_range
空字符串	返回 0（和"0"无法区分）	抛出异常
非数字字符	直接停止解析，返回已解析部分	抛出异常*/
ChatServer StatusServiceImpl::getChatServer(){
    std::lock_guard<std::mutex> guard_lock(_server_mtx);
    // 1. 默认选第一个
    auto minServer = _servers.begin()->second;
    std::string minServer_count_str = "";
    int minServer_count = 0;
    minServer_count_str = RedisMgr::GetInstance()->HGet(LOGIN_COUNT,minServer._name);
    if(minServer_count_str.empty()){
        minServer_count = INT_MAX;      //此时服务器没有启动，把它的连接数设为最大
    }else{
        minServer_count = std::stoi(minServer_count_str);
    }

    for(auto& server:_servers){
        std::string server_count_str = "";
        int server_count = 0;
        if(server.first == minServer._name)
            continue;
        server_count_str = RedisMgr::GetInstance()->HGet(LOGIN_COUNT,server.first);
        if(server_count_str.empty()){
            server_count = INT_MAX;      
        }else{
            server_count = std::stoi(server_count_str);
        }
        if(server_count < minServer_count){
            minServer = server.second;
            minServer_count  = server_count;
        }
    }
    return minServer;
}

//这里是往redis里写键值对  ： utoken_+uid , token
void StatusServiceImpl::insertToken(int uid,std::string token){
    //std::to_string(uid) 会返回一个临时的 std::string 对象
    std::string uid_string = std::to_string(uid);
    std::string token_key = USERTOKENPREFIX + uid_string;
    RedisMgr::GetInstance()->Set(token_key,token);
}
