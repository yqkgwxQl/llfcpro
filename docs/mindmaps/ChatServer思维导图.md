---

excalidraw-plugin: parsed
tags: [excalidraw]

---
==⚠  Switch to EXCALIDRAW VIEW in the MORE OPTIONS menu of this document. ⚠== You can decompress Drawing data with the command palette: 'Decompress current Excalidraw file'. For more info check in plugin settings under 'Saving'


# Excalidraw Data

## Text Elements
ChatServer ^FUsKw1V4


int main(){
    auto& cfg = ConfigMgr::GetInstance();
    auto server_name = cfg["SelfServer"]["Name"];
    try{
        auto pool = AsioIOServicePool::GetInstance();
        //将登录数设为0
        RedisMgr::GetInstance()->HSet(LOGIN_COUNT,server_name,"0");
        //创建rpc服务器
        ChatServiceImpl service;
        grpc::ServerBuilder builder;
        std::string server_address = cfg["SelfServer"]["Host"] + ":" + cfg["SelfServer"]["RPCPort"];
        builder.AddListeningPort(server_address,grpc::InsecureServerCredentials());
        builder.RegisterService(&service);
        std::unique_ptr<grpc::Server> server(builder.BuildAndStart());
        //让其运行到另一个线程上
        std::thread grpc_server_thread([&server](){
            server->Wait();
        });

        boost::asio::io_context io_context;
        boost::asio::signal_set signals(io_context,SIGINT,SIGTERM);
        signals.async_wait([&io_context,pool,&server](const boost::system::error_code,int signal_number){
            io_context.stop();
            pool->Stop();
            server->Shutdown();
        });
        // std::string host = cfg["ChatServer"]["Host"];
        // unsigned short port = static_cast<unsigned short>(std::stoi(cfg["chatserver1"]["Port"]));
        unsigned short port = static_cast<unsigned short>(std::stoi(cfg["SelfServer"]["Port"]));
        auto chatServer = std::make_shared<CServer>(io_context,port);
        chatServer->Start();
        io_context.run();
        RedisMgr::GetInstance()->HDel(LOGIN_COUNT,server_name);
        RedisMgr::GetInstance()->Close();   //那其它池要不要关？ ——> 其它池有析构，你不用关心
        grpc_server_thread.join();
    }catch (std::exception& e){
        std::cerr << "[ChatServer] error is :" << e.what() << std::endl;
    }
} ^ijOywZLP

2.创建rpc服务器并将其运行到另一线程上 ^nAtKqa8V

1.初始化AsioIOServicePool和RedisMgr(内含有RedisConPool)，并且在Redis中设置[logincount][SelfServer的name][0] ^yBHMiHya

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
}; ^ilqG8OsQ

Const.h ^ldBpf385

3.启动聊天服务器ChatServer ^ZvSFEOjt

CServer ^HnXIGq6m

class CSession;
class CServer: public std::enable_shared_from_this<CServer>{
public:
    CServer(boost::asio::io_context& ioc,unsigned short& port);
    ~CServer();
    void ClearSession(std::string session_id);
    void Start();
private:
    void StartAccept();
    void HandleAccept(std::shared_ptr<CSession> new_session,const boost::system::error_code& error);
    net::io_context& _io_context;
    short _port;
    tcp::acceptor _acceptor; 
    ///这个由session_id 和 csession 构成的_sessions有什么作用，它的作用会被UseMgr(std::unordered_map<std::size_t, std::shared_ptr<CSession>> _uid_to_session;)取代吗？？？？？？？？？？？？？？
    std::map<std::string,std::shared_ptr<CSession>> _sessions;
    std::mutex _mutex;
}; ^mHEIWVKR

void CServer::StartAccept(){
    auto self = shared_from_this();
    auto& ioc = AsioIOServicePool::GetInstance()->GetIOService();
    auto new_session = std::make_shared<CSession>(ioc,self);
    // auto new_session = std::make_shared<CSession>(ioc,this);
    _acceptor.async_accept(new_session->GetSocket(),std::bind(&HandleAccept,this,new_session,std::placeholders::_1));
}

void CServer::HandleAccept(std::shared_ptr<CSession> new_session,const boost::system::error_code& error){
    if(!error){
        new_session->Start();   //这里是异步的，不会阻塞
        std::lock_guard<std::mutex> guard_lock(_mutex);
        _sessions.insert(std::pair(new_session->GetSessionId(),new_session));
    }else{
        std::cout << "session accept failed, error is " << error.what() << std::endl;
        //这里出错实际上是Server 自身关闭、文件描述符耗尽、acceptor 被销毁等，不过出错了不用关心，引用计数归零自动销毁
    }
    StartAccept();
}

//当有连接异常时，可以调用移除用户Session的接口
void CServer::ClearSession(std::string session_id){
    auto session_iter = _sessions.find(session_id);
    if(session_iter != _sessions.end()){
        UserMgr::GetInstance()->RmvUserSession(_sessions[session_id]->GetUserId());
    }
    std::lock_guard<std::mutex> guard_lock(_mutex);
    _sessions.erase(session_id);
}    ^02g9w2DB

class CSession: public std::enable_shared_from_this<CSession>{
public:
    CSession(boost::asio::io_context& ioc,std::shared_ptr<CServer>);
    ~CSession();
    tcp::socket& GetSocket();
    std::string& GetSessionId();
    void SetUserId(int uid);
    std::size_t GetUserId();
    void Start();  
    void Send(short msg_id,std::string msg_data);

private:
    using handler = std::function<void(const boost::system::error_code& ec, std::size_t bytesTransfered)>;
    void AsyncReadHead();
    void AsyncReadBody(int msg_len);
    void AsyncReadFull(std::size_t len,handler);
    void AsyncReadLen(std::size_t has_read_len, std::size_t total_len,handler);
    void HandleWrite(const boost::system::error_code& error, std::shared_ptr<CSession> shared_self);
    void Close();
    // void DealExceptionSession();
    std::string _session_id;
    std::size_t _user_uid;
    tcp::socket _socket;
    std::shared_ptr<CServer> _server;

    //_data是实际的异步读写场所 _recv_head_node和_recv_msg_node 只负责接收数据
    char _data[MAX_LENGTH];
    // bool _b_close;
    //session 锁
        std::mutex _session_mtx;
    /*如果没有锁，两个线程同时进 Close()：一个在 _socket.close()，另一个也 _socket.close()，虽然不会崩溃但不够干净。
    _session_mtx 保证同一时刻只有一个线程在执行关闭操作。*/
    std::queue<std::shared_ptr<SendNode>> _send_que;
    std::mutex _send_lock;
    //收到的消息结构
    std::shared_ptr<RecvNode> _recv_msg_node;
    // bool _b_head_parse;
    //收到的头部结构
    std::shared_ptr<MsgNode> _recv_head_node;

};

class LogicSystem;
class LogicNode{
    friend class LogicSystem;
public:
    LogicNode(std::shared_ptr<CSession> csession,std::shared_ptr<RecvNode> recvNode);
private:
    std::shared_ptr<CSession> _csession;
    std::shared_ptr<RecvNode> _recvNode;
}; ^P2r2FWNJ

void CSession::Start(){
    AsyncReadHead();
}

void CSession::AsyncReadHead(){
    auto self = shared_from_this();
    AsyncReadFull(HEAD_TOTAL_LEN,[this,self](const boost::system::error_code& ec, std::size_t bytesTransfered){
        try{
            if (ec) {
                                std::cout << "handle read failed, error is " << ec.what() << std::endl;
                //这里实际上只传self也是可以的
                                // self->Close();
                // self->_server->ClearSessions(self->GetSessionId());
                Close();
                _server->ClearSession(_session_id);
                                // DealExceptionSession();
                                return;
                        }
            if(bytesTransfered < HEAD_TOTAL_LEN){
                std::cout << "read length not match, read [" << bytesTransfered << "] , total ["<< HEAD_TOTAL_LEN << "]" << std::endl;
                Close();
                _server->ClearSession(_session_id);
                return;
            }
            //进行id和长度的解析
            _recv_head_node->Clear();
            memcpy(_recv_head_node->_data,_data,HEAD_TOTAL_LEN);
            short msg_id = 0;
            memcpy(&msg_id,_recv_head_node->_data,HEAD_ID_LEN);
            msg_id = boost::asio::detail::socket_ops::network_to_host_short(msg_id);
            short msg_len = 0;
            memcpy(&msg_len,_recv_head_node->_data + HEAD_ID_LEN,HEAD_DATA_LEN);
            msg_len = boost::asio::detail::socket_ops::network_to_host_short(msg_len);

            std::cout << "msg_id is " << msg_id << std::endl;
                        //id非法
                        if (msg_id > MAX_LENGTH) {
                                std::cout << "invalid msg_id is " << msg_id << std::endl;
                Close();
                                _server->ClearSession(_session_id);
                                return;
                        }
            std::cout << "msg_len is " << msg_id << std::endl;
                        //len非法
                        if (msg_len > MAX_LENGTH) {
                                std::cout << "invalid msg_len is " << msg_len << std::endl;
                Close();
                                _server->ClearSession(_session_id);
                                return;
                        }
            
            _recv_msg_node = std::make_shared<RecvNode>(msg_len,msg_id);
            AsyncReadBody(msg_len);

        }catch(std::exception& e){
            std::cout << "Exception code is " << e.what() << std::endl;
        }
    });
}

void CSession::AsyncReadBody(int msg_len){
    auto self = shared_from_this();
    AsyncReadFull(msg_len,[this,self,msg_len](const boost::system::error_code& ec,std::size_t bytesTransfered){
        try{
            if(ec){
                std::cout << "handle read failed, error is " << ec.what() << std::endl;
                Close();
                _server->ClearSession(_session_id);
            }
            if(bytesTransfered < msg_len){
                std::cout << "read length not match, read [" << bytesTransfered << "] , total ["<< msg_len << "]" << std::endl;
                Close();
                _server->ClearSession(_session_id);
                return;
            }
            memcpy(_recv_msg_node->_data,_data,msg_len);
            _recv_msg_node->_cur_len += bytesTransfered;
            _recv_msg_node->_data[_recv_msg_node->_total_len] = '\0';
            std::cout << "receive data is " << _recv_msg_node->_data << std::endl;
            //此处将消息投递到逻辑队列中
            LogicSystem::GetInstance()->PostMsgToQue(std::make_shared<LogicNode>(shared_from_this(), _recv_msg_node));
            //继续监听头部接受事件
            AsyncReadHead();
        }catch(std::exception& e){
            std::cerr << "Exception is :" << e.what() << std::endl;
        }
    });
}

//这里会保证一定读全，而且不超
void CSession::AsyncReadFull(std::size_t len,handler real_handler){
    memset(_data,0,MAX_LENGTH);
    AsyncReadLen(0,len,real_handler);
}

//读取指定字节数
void CSession::AsyncReadLen(std::size_t has_read_len, std::size_t total_len,handler real_handler){
    auto self = shared_from_this(); 
    //&has_read_len ×××××××××× 这里传引用绝对会有问题，因为是异步的，有可能作用域消失直接析构，你还用引用，那就绝对有问题
    _socket.async_read_some(boost::asio::buffer(_data + has_read_len,total_len - has_read_len)
    ,[self,has_read_len,total_len,real_handler,this](const boost::system::error_code& ec, std::size_t bytesTransfered){
        if(ec){
            // 出现错误，调用回调函数
            real_handler(ec, has_read_len + bytesTransfered);
            return;
        }
        // 这里实际上has_read_len + bytesTransfered == total_len也可以   防御性编程
        if(has_read_len + bytesTransfered >= total_len){
            real_handler(ec,bytesTransfered + has_read_len);
            // real_handler(ec,total_len);
            return;
        }
        //这里长度不够
        AsyncReadLen(has_read_len + bytesTransfered,total_len,real_handler);
    });
}

void CSession::Close(){
    std::lock_guard<std::mutex> guard_lock(_session_mtx);
    _socket.close();
}
void CSession::Send(short msg_id,std::string msg_data){
    std::unique_lock<std::mutex> unique_lock(_send_lock);
    if(_send_que.size() > MAX_SENDQUE){
        std::cerr << "ChatServer send_queue is full, can not add data" << std::endl;
        return;
    }
    //所以这里 _send_que.size() = 1 ——> 写1
    //        _send_que.size() > 1 ————>  写2（在写1中实现了）
    _send_que.push(std::make_shared<SendNode>(msg_data.c_str(),msg_data.length(),msg_id));
    if(_send_que.size() > 1){
        return;
    }
        auto& msgnode = _send_que.front();
    //在 C++ 中，普通函数和成员函数的行为不一样：普通函数：函数名可以隐式转为指针
    //成员函数：函数名不能隐式转为指针   std::bind(HandleWrite,this,std::placeholders::_1,shared_from_this()) ××××××××
    boost::asio::async_write(_socket,boost::asio::buffer(msgnode->_data,msgnode->_total_len),
    std::bind(&HandleWrite,this,std::placeholders::_1,shared_from_this()));
}

//boost::system::error_code — asio 异步操作的错误   这是 async_write 操作本身的错误码，由 Boost.Asio 在回调中传入
//std::exception — C++ 异常     场景                                        说明
                    //memcpy 访问非法地址                            _data 是空指针或者长度不对
                    // shared_from_this() 抛 bad_weak_ptr            对象没有被 shared_ptr 管理就在用 enable_shared_from_this
void CSession::HandleWrite(const boost::system::error_code& error, std::shared_ptr<CSession> shared_self){
    try{
        if(!error){
            std::unique_lock<std::mutex> unique_lock(_send_lock);
            _send_que.pop();
            if(!_send_que.empty()){
                auto& msgnode = _send_que.front();
                boost::asio::async_write(_socket,boost::asio::buffer(msgnode->_data,msgnode->_total_len)
                ,std::bind(&HandleWrite,this,std::placeholders::_1,shared_from_this()));
            }
        }else{
            std::cerr << "[ChatServer]error is :" << error.what() << std::endl;
        }
    }catch (std::exception& e){
        std::cerr << "Exceptin code is :" << e.what() << std::endl;
    }
}

LogicNode::LogicNode(std::shared_ptr<CSession> session,std::shared_ptr<RecvNode> recvNode)
    :_csession(session),_recvNode(recvNode){

} ^asiT7bRD

CSession ^1j2o1OcL

session->Start() → AsyncReadHead → AsyncReadFull → async_read_some
 ^EjLcjss0

这里的Send只是把消息发送到发送队列里并write第一个msg，它里面的回调HandleWrite会真正写完剩余的所有msg ^aXjf2jQP

LogicSystem ^W7NsLgWe

session ^YYQsRgSV

using FunCallBack = std::function<void(std::shared_ptr<CSession>,const std::string& msg_data)>;

class LogicSystem: public Singleton<LogicSystem>{

    friend class Singleton<LogicSystem>;
public:
    ~LogicSystem();
    void PostMsgToQue(std::shared_ptr<LogicNode> logic_node);
    void DealMsg();
    bool GetBaseInfo(std::string base_key, int uid, std::shared_ptr<UserInfo> &userinfo);
private:
    void RejesterCallBack();
    LogicSystem();
    void LoginHandler(std::shared_ptr<CSession>,const std::string& msg_data);
    bool _b_stop;
    std::thread _work_thread;
    //logic_request_queues ————> 逻辑请求队列 这样写更好
    std::queue<std::shared_ptr<LogicNode>> _logic_queues;
    std::mutex _mutex;
    std::condition_variable _cond;
    std::map<short,FunCallBack> _callBacks;
}; ^edojzvPE



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
    std::string count_str = "";
    count_str = RedisMgr::GetInstance()->HGet(LOGIN_COUNT,server_name);
    std::size_t count = std::atoi(count_str.c_str());
    count++;
    RedisMgr::GetInstance()->HSet(LOGIN_COUNT,server_name,std::to_string(count));
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
 ^bxidcjuU

这里对应的第一次:redis中的<userinfo_key : user_info(序列化数据)> ^fPe87ddV

CSession里客户端request(id + len + 序列化请求数据) -> _recv_msg_node(id + 序列化请求数据) -> LogicNode( session + _recv_msg_node ) ^7idLE5Vv

LogicSystem::GetInstance()->PostMsgToQue() ^V1ztMegB

根据_recv_msg_node的id寻找对应的回调 ^7LmNVsTh

在回调里，反序列化 序列化请求数据，得到请求数据 ——> 进行处理 ———> 组装响应数据 ——> 序列化响应数据 ——> 进行发送,session->Send()  ^KvoOn5Wf

正好照应了规范：逻辑层完成 反序列化 + 处理 + 序列化 ^7pMTAn1u

这样才能发送给Session层既有id，又有序列化数据 ^cGZdInHV

Grpc ^BGvAq3bB

syntax = "proto3";

package message;

service VarifyService {
  rpc GetVarifyCode (GetVarifyReq) returns (GetVarifyRsp) {}
}

message GetVarifyReq {
  string email = 1;
}

message GetVarifyRsp {
  int32 error = 1;
  string email = 2;
  string code = 3;
}


message GetChatServerReq {
  int32 uid = 1;
}

message GetChatServerRsp {
  int32 error = 1;
  string host = 2;
  string port = 3;
  string token = 4;
}

message LoginReq{
        int32 uid = 1;
        string token= 2;
}

message LoginRsp {
        int32 error = 1;
        int32 uid = 2;
        string token = 3;
}

service StatusService {
    rpc GetChatServer (GetChatServerReq) returns (GetChatServerRsp) {}
    rpc Login (LoginReq) returns (LoginRsp) {};
}

// 模块一：好友申请
message AddFriendReq {
    int32 applyuid = 1;   // 申请人 uid
    string name = 2;      // 申请人姓名
    string desc = 3;      // 申请附言
    string icon = 4;      // 申请人头像
    string nick = 5;      // 申请人昵称
    int32  sex = 6;       // 申请人性别
    int32  touid = 7;     // 被申请人 uid
}

message AddFriendRsp {
    int32 error = 1;
    int32 applyuid = 2;  // 谁申请的（申请人）
    int32 touid = 3;     // 被申请的人
}
//流程： A（在 ChatServer1）申请加 B 为好友 → ChatServer1 通过 gRPC 通知 ChatServer2 → B 收到好友申请通知。

//模块二：好友申请回复
message RplyFriendReq {
    int32 rplyuid = 1;   // 回复人 uid（被申请人）
    bool  agree = 2;     // 是否同意
    int32 touid = 3;     // 对方 uid（申请人）
}

message RplyFriendRsp {
    int32 error = 1;
    int32 rplyuid = 2;
    int32 touid = 3;
}
// 流程： B 同意/拒绝 A 的好友申请 → ChatServer2 通过 gRPC 通知 ChatServer1 → A 收到回复结果。

//模块三：聊天消息
  // 3.1 简单文本消息: 简单的单条消息发送，数据量小。
  message SendChatMsgReq {
    int32 fromuid = 1;
    int32 touid = 2;
    string message = 3;
}

message SendChatMsgRsp {
    int32 error = 1;
    int32 fromuid = 2;
    int32 touid = 3;
}
  // 3.2 多段文本消息（增强版）: 支持多段消息一次性发送，附带 thread_id 和 unique_id，适合带线程/会话的聊天系统。
message TextChatData {
  string unique_id = 1;    // 客户端生成的唯一 id，用于去重
  int32 msg_id = 2;        // 服务器分配的消息 id
  string msgcontent = 3;   // 消息内容
  string chat_time = 4;    // 发送时间
}

message TextChatMsgReq {
    int32 fromuid = 1;
    int32 touid = 2;
    int32 thread_id = 3;                  // 聊天线程 id
    repeated TextChatData textmsgs = 4;   // 多条消息
}

message TextChatMsgRsp {
    int32 error = 1;
    int32 fromuid = 2;
    int32 touid = 3;
    int32 thread_id = 4;
    repeated TextChatData textmsgs = 5;   // 返回已保存的消息信息
}

//模块四：踢人
message KickUserReq {
    int32 uid = 1;     // 要被踢的用户
}

message KickUserRsp {
    int32 error = 1;
    int32 uid = 2;
}
//场景： 用户 A 在 ChatServer2 登录着，又在 ChatServer1 重新登录 → ChatServer1 通过 gRPC 通知 ChatServer2 把 A 踢下线。

//模块五：好友认证（收尾）
message AddFriendMsg {
    int32 sender_id = 1;
    string unique_id = 2;
    int32 msg_id = 3;
    int32 thread_id = 4;
    string msgcontent = 5;
    int32 status = 6;
}

message AuthFriendReq {
    int32 fromuid = 1;
    int32 touid = 2;
    repeated AddFriendMsg textmsgs = 3;   // 附带的聊天记录
}

message AuthFriendRsp {
    int32 error = 1;
    int32 fromuid = 2;
    int32 touid = 3;
}
//流程： B 同意好友申请后 → 告诉 A"我们已经是好友了"，同时携带之前离线期间的历史消息。

service ChatService{
        rpc NotifyAddFriend(AddFriendReq) returns (AddFriendRsp) {}
        rpc RplyAddFriend(RplyFriendReq) returns (RplyFriendRsp) {}
        rpc SendChatMsg(SendChatMsgReq) returns (SendChatMsgRsp) {}
        rpc NotifyTextChatMsg(TextChatMsgReq) returns (TextChatMsgRsp){}
        rpc NotifyKickUser(KickUserReq) returns (KickUserRsp){}
        rpc NotifyAuthFriend(AuthFriendReq) returns (AuthFriendRsp) {}
} ^roCNSJ58

Stub ^uzexz5o4

Server ^uapAa0xf

[GateServer]GetVarifyReq ^bVY49Xxl

[VarifyServer]GetVarifyRsp ^dWMHYabt

[GateServer]GetChatServerReq ^G1VQCaFw

[StatusServer]GetChatServerRsp ^j6ILQRtt

[ChatServer]LoginReq ^egnxOG7b

[StatusServer]LoginRsp ^xzvHTLhs

需注意的是客户端连接ChatServer是连接的ChatServer的一个Session,一个ChatServer可以有多个Session，分别对应多个用户的客户端 ^5rB5XQl4

这个时候虽然客户端是和这个Session相连接了，但是此时Session也就是ChatServer的这个Session还并不认识这个客户端 ^132EiFo7

1 ^wNddUldB

2 ^7EpdkvnT

经过token验证后，这里ChatServer的这个Session才真正认识了客户端 ^h0YbYovt

因为这里是一个个取出ID后继续进行处理，决定了这里可以使用引用 ^tlZin65w

%%
## Drawing
```compressed-json
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
```
%%