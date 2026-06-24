#include "RedisMgr.h"
#include <cstring>
#include "ConfigMgr.h"

RedisMgr::RedisMgr(){
    ConfigMgr& CfgMgr = ConfigMgr::GetInstance();
    std::string host = CfgMgr["Redis"]["Host"];
    std::string port = CfgMgr["Redis"]["Port"];
    std::string pwd = CfgMgr["Redis"]["Passwd"];
    // pool_ = std::unique_ptr<RPConPool>(new RPConPool(5,host,port));
    // pool_.reset(new RPConPool(5, host, port));
    _con_pool = std::make_unique<RedisConPool>(host.c_str(),std::stoi(port),5,pwd.c_str());
}

RedisMgr::~RedisMgr(){

}

// //连接指定主机和端口的 Redis 服务器-------------------
// bool RedisMgr::Connect(const std::string &host, int port){
//     this->_connect = redisConnect(host.c_str(), port);
//     if (this->_connect == NULL){
//         std::cout << "connect empty " << std::endl;
//         return false;
//     }
//     if (this->_connect->err){
//         std::cout << "connect error " << this->_connect->errstr << std::endl;
//         return false;
//     }
//     return true;
// }

//获取key对应的value-------------------
//通过 redisCommand() 发送 GET key 命令
bool RedisMgr::Get(const std::string &key, std::string& value)
{
    auto context = _con_pool->GetRedisConnection();
    if(context == nullptr){
        return false;
    }
    /*using RedisContextPtr = std::unique_ptr<redisContext, RedisContextDeleter>; ——> 模板参数就是 redisContext
     RedisContextPtr.get()返回的不是C风格的原生指针，而是redisContext*  */
    //redisCommand ————> 向 Redis 服务器发送一条命令，并返回执行结果redisReply*
    auto reply = (redisReply*)redisCommand(context.get(), "GET %s", key.c_str());
    if (reply == NULL) {
        std::cout << "[ GET  " << key << " ] failed" << std::endl;
        freeReplyObject(reply);
        _con_pool->ReturnRedisConnection(std::move(context));
        return false;
    }

     if (reply->type != REDIS_REPLY_STRING) {
         std::cout << "[ GET  " << key << " ] failed" << std::endl;
         freeReplyObject(reply);
        _con_pool->ReturnRedisConnection(std::move(context));
         return false;
    }

     value = reply->str;
     freeReplyObject(reply);

     std::cout << "Succeed to execute command [ GET " << key << "  ]" << std::endl;
     _con_pool->ReturnRedisConnection(std::move(context));
     return true;
}

//设置key和value-------------------
bool RedisMgr::Set(const std::string &key, const std::string &value){
    //执行redis命令行
    auto context = _con_pool->GetRedisConnection();
    if(context == nullptr){
        return false;
    }
    auto reply = (redisReply*)redisCommand(context.get(), "SET %s %s", key.c_str(), value.c_str());

    //如果返回NULL则说明执行失败
    if (NULL == reply)
    {
        std::cout << "Execut command [ SET " << key << "  "<< value << " ] failure ! " << std::endl;
        freeReplyObject(reply);
        _con_pool->ReturnRedisConnection(std::move(context));
        return false;
    }

    //如果执行失败则释放连接
    if (!(reply->type == REDIS_REPLY_STATUS && (strcmp(reply->str, "OK") == 0 || strcmp(reply->str, "ok") == 0)))
    {   
        //这段代码检查两个条件：
        // 返回类型必须是 REDIS_REPLY_STATUS
        // 返回的字符串内容必须是 "OK" 或 "ok"（兼容大小写）
        std::cout << "Execut command [ SET " << key << "  " << value << " ] failure ! " << std::endl;
        freeReplyObject(reply);     
        _con_pool->ReturnRedisConnection(std::move(context));
        return false;
    }

    //执行成功 释放redisCommand执行后返回的redisReply所占用的内存
    freeReplyObject(reply);
    std::cout << "Execut command [ SET " << key << "  " << value << " ] success ! " << std::endl;
     _con_pool->ReturnRedisConnection(std::move(context));
    return true;
}

//密码认证-------------------
// bool RedisMgr::Auth(const std::string &password)
// {
//     this->_reply = (redisReply*)redisCommand(this->_connect, "AUTH %s", password.c_str());
//     if (this->_reply->type == REDIS_REPLY_ERROR) {
//         std::cout << "认证失败" << std::endl;
//         //执行成功 释放redisCommand执行后返回的redisReply所占用的内存
//         freeReplyObject(this->_reply);
//         return false;
//     }
//     else {
//         //执行成功 释放redisCommand执行后返回的redisReply所占用的内存
//         freeReplyObject(this->_reply);
//         std::cout << "认证成功" << std::endl;
//         return true;
//     }
// }

//左侧push-------------------
bool RedisMgr::LPush(const std::string &key, const std::string &value)
{
    auto context = _con_pool->GetRedisConnection();
    if(context == nullptr){
        return false;
    }
    auto reply = (redisReply*)redisCommand(context.get(), "LPUSH %s %s", key.c_str(), value.c_str());
    if (NULL == reply)
    {
        std::cout << "Execut command [ LPUSH " << key << "  " << value << " ] failure ! " << std::endl;
        freeReplyObject(reply);
        _con_pool->ReturnRedisConnection(std::move(context));
        return false;
    }

    if (reply->type != REDIS_REPLY_INTEGER || reply->integer <= 0) {
        std::cout << "Execut command [ LPUSH " << key << "  " << value << " ] failure ! " << std::endl;
        freeReplyObject(reply);
        _con_pool->ReturnRedisConnection(std::move(context));
        return false;
    }

    std::cout << "Execut command [ LPUSH " << key << "  " << value << " ] success ! " << std::endl;
    freeReplyObject(reply);
     _con_pool->ReturnRedisConnection(std::move(context));
    return true;
}

//左侧pop-------------------
bool RedisMgr::LPop(const std::string &key, std::string& value){
    auto context = _con_pool->GetRedisConnection();
    if(context == nullptr){
        return false;
    }
    auto reply = (redisReply*)redisCommand(context.get(), "LPOP %s ", key.c_str());
    if (reply == nullptr || reply->type == REDIS_REPLY_NIL) {
        std::cout << "Execut command [ LPOP " << key<<  " ] failure ! " << std::endl;
        freeReplyObject(reply);
        _con_pool->ReturnRedisConnection(std::move(context));
        return false;
    }
    value = reply->str;
    std::cout << "Execut command [ LPOP " << key <<  " ] success ! " << std::endl;
    freeReplyObject(reply);
     _con_pool->ReturnRedisConnection(std::move(context));
    return true;
}

//右侧pop-------------------
bool RedisMgr::RPush(const std::string& key, const std::string& value) {
    auto context = _con_pool->GetRedisConnection();
    if(context == nullptr){
        return false;
    }
    auto reply = (redisReply*)redisCommand(context.get(), "RPUSH %s %s", key.c_str(), value.c_str());
    if (NULL == reply)
    {
        std::cout << "Execut command [ RPUSH " << key << "  " << value << " ] failure ! " << std::endl;
        freeReplyObject(reply);
        _con_pool->ReturnRedisConnection(std::move(context));
        return false;
    }

    if (reply->type != REDIS_REPLY_INTEGER || reply->integer <= 0) {
        std::cout << "Execut command [ RPUSH " << key << "  " << value << " ] failure ! " << std::endl;
        freeReplyObject(reply);
        _con_pool->ReturnRedisConnection(std::move(context));
        return false;
    }

    std::cout << "Execut command [ RPUSH " << key << "  " << value << " ] success ! " << std::endl;
    freeReplyObject(reply);
    _con_pool->ReturnRedisConnection(std::move(context));
    return true;
}
//右侧pop-------------------
bool RedisMgr::RPop(const std::string& key, std::string& value) {
    auto context = _con_pool->GetRedisConnection();
    if(context == nullptr){
        return false;
    }
    auto reply = (redisReply*)redisCommand(context.get(), "RPOP %s ", key.c_str());
    if (reply == nullptr || reply->type == REDIS_REPLY_NIL) {
        std::cout << "Execut command [ RPOP " << key << " ] failure ! " << std::endl;
        freeReplyObject(reply);
        _con_pool->ReturnRedisConnection(std::move(context));
        return false;
    }
    value = reply->str;
    std::cout << "Execut command [ RPOP " << key << " ] success ! " << std::endl;
    freeReplyObject(reply);
    _con_pool->ReturnRedisConnection(std::move(context));
    return true;
}

//HSet操作 ——> Hash Set -------------------
  /*假设你要存储一个用户信息:
        HSET user:1001 name "Alice"
        HSET user:1001 age "25"
        HSET user:1001 email "alice@example.com"
    执行后:
        user:1001
        └── name  → "Alice"
        └── age   → "25"
        └── email → "alice@example.com"*/
bool RedisMgr::HSet(const std::string &key, const std::string &hkey, const std::string &value) {
    auto context = _con_pool->GetRedisConnection();
    if(context == nullptr){
        return false;
    }
    auto reply = (redisReply*)redisCommand(context.get(), "HSET %s %s %s", key.c_str(), hkey.c_str(), value.c_str());
    if (reply == nullptr ) {
		std::cout << "Execut command [ HSet " << key << " "<< hkey <<"  ] failure ! " << std::endl;
		_con_pool->ReturnRedisConnection(std::move(context));
		return false;
	}

	if ( reply->type != REDIS_REPLY_INTEGER) {
		freeReplyObject(reply);
		std::cout << "Execut command [ HSet " << key << " " << hkey << "  ] failure ! " << std::endl;
		_con_pool->ReturnRedisConnection(std::move(context));
		return false;
	}
    std::cout << "Execut command [ HSet " << key << "  " << hkey << "  " << value << " ] success ! " << std::endl;
    freeReplyObject(reply);
    _con_pool->ReturnRedisConnection(std::move(context));
    return true;
}


bool RedisMgr::HSet(const char* key, const char* hkey, const char* hvalue, size_t hvaluelen)
{
    const char* argv[4];
    size_t argvlen[4];
    argv[0] = "HSET";
    argvlen[0] = 4;
    argv[1] = key;
    argvlen[1] = strlen(key);
    argv[2] = hkey;
    argvlen[2] = strlen(hkey);
    argv[3] = hvalue;
    argvlen[3] = hvaluelen;
    auto context = _con_pool->GetRedisConnection();
    if(context == nullptr){
        return false;
    }
    auto reply = (redisReply*)redisCommandArgv(context.get(), 4, argv, argvlen);
    if (reply == nullptr) {
        std::cout << "[ HSet " << key << " " << hkey << " ] network error" << std::endl;
        _con_pool->ReturnRedisConnection(std::move(context));
        return false;
    }

    if (reply->type != REDIS_REPLY_INTEGER) {
        freeReplyObject(reply);
        std::cout << "[ HSet " << key << " " << hkey << " ] failed" << std::endl;
        _con_pool->ReturnRedisConnection(std::move(context));
        return false;
    }
    std::cout << "Execut command [ HSet " << key << "  " << hkey << "  " << hvalue << " ] success ! " << std::endl;
    freeReplyObject(reply);
    _con_pool->ReturnRedisConnection(std::move(context));
    return true;
}

//HGet操作-------------------
std::string RedisMgr::HGet(const std::string &key, const std::string &hkey)
{
    const char* argv[3];
    size_t argvlen[3];
    argv[0] = "HGET";
    argvlen[0] = 4;
    argv[1] = key.c_str();
    argvlen[1] = key.length();
    argv[2] = hkey.c_str();
    argvlen[2] = hkey.length();
    auto context = _con_pool->GetRedisConnection();
    if(context == nullptr){
        return "";
    }
    auto reply = (redisReply*)redisCommandArgv(context.get(), 3, argv, argvlen);
    if (reply == nullptr) {
        std::cout << "[ HGet " << key << " " << hkey << " ] network error" << std::endl;
        _con_pool->ReturnRedisConnection(std::move(context));
        return "";
    }

    if (reply->type == REDIS_REPLY_NIL) {
        freeReplyObject(reply);
        std::cout << "[ HGet " << key << " " << hkey << " ] key not found" << std::endl;
        _con_pool->ReturnRedisConnection(std::move(context));
        return "";
    }

    std::string value = reply->str;
    freeReplyObject(reply);
    std::cout << "Execut command [ HGet " << key << " " << hkey << " ] success ! " << std::endl;
    _con_pool->ReturnRedisConnection(std::move(context));
    return value;
}

//Del 操作-------------------
bool RedisMgr::Del(const std::string &key)
{
    auto context = _con_pool->GetRedisConnection();
    if(context == nullptr){
        return false;
    }
    auto reply = (redisReply*)redisCommand(context.get(), "DEL %s", key.c_str());
    if (reply == nullptr || reply->type != REDIS_REPLY_INTEGER) {
        std::cout << "Execut command [ Del " << key <<  " ] failure ! " << std::endl;
        freeReplyObject(reply);
        _con_pool->ReturnRedisConnection(std::move(context));
        return false;
    }
    //这样调用者能区分"删了 1 个"和"key 不存在"。
    bool deleted = (reply->integer > 0);
    std::cout << "[ DEL " << key << " ] " << (deleted ? "deleted" : "key not found")
              << " (" << reply->integer << ")" << std::endl;
     freeReplyObject(reply);
    _con_pool->ReturnRedisConnection(std::move(context));
     return deleted;
}

//判断键值是否存在-------------------
bool RedisMgr::ExistsKey(const std::string &key)
{
    auto context = _con_pool->GetRedisConnection();
    if(context == nullptr){
        return false;
    }
    auto reply = (redisReply*)redisCommand(context.get(), "exists %s", key.c_str());
    if (reply == nullptr || reply->type != REDIS_REPLY_INTEGER || reply->integer == 0) {
        std::cout << "Not Found [ Key " << key << " ]  ! " << std::endl;
        freeReplyObject(reply);
        _con_pool->ReturnRedisConnection(std::move(context));
        return false;
    }
    bool exists = (reply->integer > 0);
    std::cout << "[ EXISTS " << key << " ] " << (exists ? "found" : "not found") << std::endl;
    freeReplyObject(reply);
    _con_pool->ReturnRedisConnection(std::move(context));
    return exists;
}

//关闭-------------------
void RedisMgr::Close()
{
    _con_pool->Close();
    _con_pool->ClearConnections();
}

