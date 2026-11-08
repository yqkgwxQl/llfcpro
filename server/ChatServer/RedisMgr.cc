#include "RedisMgr.h"
#include "Logger.h"
#include <cstring>
#include "ConfigMgr.h"
#include "const.h"

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
/*Redis 的 GET 命令可能的返回类型：
Redis        返回	                reply->type	含义
"hello"	REDIS_REPLY_STRING	    找到了，值是字符串
(nil)	REDIS_REPLY_NIL	key     不存在
-ERR	REDIS_REPLY_ERROR	    命令错了*/
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
        LOGI << "[ GET  " << key << " ] failed" << std::endl;
        // freeReplyObject(reply);
        _con_pool->ReturnRedisConnection(std::move(context));
        return false;
    }

     if (reply->type != REDIS_REPLY_STRING) {
         LOGI << "[ GET  " << key << " ] failed" << std::endl;
         freeReplyObject(reply);
        _con_pool->ReturnRedisConnection(std::move(context));
         return false;
    }

     value = reply->str;
     freeReplyObject(reply);

     LOGI << "Succeed to execute command [ GET " << key << "  ]" << std::endl;
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
        LOGI << "Execut command [ SET " << key << "  "<< value << " ] failure ! " << std::endl;
        // freeReplyObject(reply);
        _con_pool->ReturnRedisConnection(std::move(context));
        return false;
    }

    //如果执行失败则释放连接
    if (!(reply->type == REDIS_REPLY_STATUS && (strcmp(reply->str, "OK") == 0 || strcmp(reply->str, "ok") == 0)))
    {   
        //这段代码检查两个条件：
        // 返回类型必须是 REDIS_REPLY_STATUS
        // 返回的字符串内容必须是 "OK" 或 "ok"（兼容大小写）
        LOGI << "Execut command [ SET " << key << "  " << value << " ] failure ! " << std::endl;
        freeReplyObject(reply);     
        _con_pool->ReturnRedisConnection(std::move(context));
        return false;
    }

    //执行成功 释放redisCommand执行后返回的redisReply所占用的内存
    freeReplyObject(reply);
    LOGI << "Execut command [ SET " << key << "  " << value << " ] success ! " << std::endl;
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
    //LPUSH 的返回值是插入后列表的总长度。
    auto reply = (redisReply*)redisCommand(context.get(), "LPUSH %s %s", key.c_str(), value.c_str());
    if (NULL == reply)
    {
        LOGI << "Execut command [ LPUSH " << key << "  " << value << " ] failure ! " << std::endl;
        // freeReplyObject(reply);
        _con_pool->ReturnRedisConnection(std::move(context));
        return false;
    }

    if (reply->type != REDIS_REPLY_INTEGER || reply->integer <= 0) {
        LOGI << "Execut command [ LPUSH " << key << "  " << value << " ] failure ! " << std::endl;
        freeReplyObject(reply);
        _con_pool->ReturnRedisConnection(std::move(context));
        return false;
    }

    LOGI << "Execut command [ LPUSH " << key << "  " << value << " ] success ! " << std::endl;
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
    /*	                reply == nullptr	        reply->type == REDIS_REPLY_NIL
    含义	            命令没发出去	                命令成功了，但没数据
    原因	            网络断、内存不足	            列表为空、key 不存在
    能访问 reply 吗？	❌ 不能，会崩溃	                ✅ 可以
    能读 reply->str 吗？	❌ 崩溃	                ⚠️ 不可以，reply->str 也是 nullptr
    对应的 Redis 输出	    没有输出	                    (nil)
    --------------------------------
    redisCommand("LPOP mylist")
    │
    ├── TCP 连接断开 → redisCommand 返回 nullptr
    │     ↓
    │   if (reply == nullptr) → 网络异常 → return false
    │
    └── TCP 正常，Redis 收到命令
          │
          ├── mylist 里有元素
          │     ↓
          │   reply->type = REDIS_REPLY_STRING
          │   reply->str  = "hello"
          │   → value = "hello" ✅
          │
          └── mylist 是空的
                ↓
              reply->type = REDIS_REPLY_NIL
              reply->str  = nullptr
              → 条件成立 → return false ❌*/
    if (reply == nullptr ) {
        LOGI << "Execut command [ LPOP " << key<<  " ] failure ! " << std::endl;
        // freeReplyObject(reply);
        _con_pool->ReturnRedisConnection(std::move(context));
        return false;
    }
    if (reply->type == REDIS_REPLY_NIL) {
        LOGI << "Execut command [ LPOP " << key<<  " ] not found ! " << std::endl;
        freeReplyObject(reply);
        _con_pool->ReturnRedisConnection(std::move(context));
        return false;
    }
    
    value = reply->str;
    LOGI << "Execut command [ LPOP " << key <<  " ] success ! " << std::endl;
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
        LOGI << "Execut command [ RPUSH " << key << "  " << value << " ] failure ! " << std::endl;
        // freeReplyObject(reply);
        _con_pool->ReturnRedisConnection(std::move(context));
        return false;
    }

    if (reply->type != REDIS_REPLY_INTEGER || reply->integer <= 0) {
        LOGI << "Execut command [ RPUSH " << key << "  " << value << " ] failure ! " << std::endl;
        freeReplyObject(reply);
        _con_pool->ReturnRedisConnection(std::move(context));
        return false;
    }

    LOGI << "Execut command [ RPUSH " << key << "  " << value << " ] success ! " << std::endl;
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
    if (reply == nullptr) {
        LOGI << "Execut command [ RPOP " << key << " ] failure ! " << std::endl;
        // freeReplyObject(reply);
        _con_pool->ReturnRedisConnection(std::move(context));
        return false;
    }
    if (reply->type == REDIS_REPLY_NIL) {
        LOGI << "Execut command [ RPOP " << key << " ] failure ! " << std::endl;
        freeReplyObject(reply);
        _con_pool->ReturnRedisConnection(std::move(context));
        return false;
    }
    value = reply->str;
    LOGI << "Execut command [ RPOP " << key << " ] success ! " << std::endl;
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
		LOGI << "Execut command [ HSet " << key << " "<< hkey <<"  ] failure ! " << std::endl;
		_con_pool->ReturnRedisConnection(std::move(context));
		return false;
	}

    /*HSET 的返回值
    HSET key field value 执行成功后，Redis 返回一个整数：
    情况	                Redis 返回	    含义
    field 不存在，新插入	(integer) 1	    新增了 1 个字段
    field 已存在，覆盖旧值	(integer) 0	    没有新增字段，只是更新*/
	if ( reply->type != REDIS_REPLY_INTEGER) {
		freeReplyObject(reply);
		LOGI << "Execut command [ HSet " << key << " " << hkey << "  ] failure ! " << std::endl;
		_con_pool->ReturnRedisConnection(std::move(context));
		return false;
	}
    LOGI << "Execut command [ HSet " << key << "  " << hkey << "  " << value << " ] success ! " << std::endl;
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
        LOGI << "[ HSet " << key << " " << hkey << " ] network error" << std::endl;
        _con_pool->ReturnRedisConnection(std::move(context));
        return false;
    }

    if (reply->type != REDIS_REPLY_INTEGER) {
        freeReplyObject(reply);
        LOGI << "[ HSet " << key << " " << hkey << " ] failed" << std::endl;
        _con_pool->ReturnRedisConnection(std::move(context));
        return false;
    }
    LOGI << "Execut command [ HSet " << key << "  " << hkey << "  " << hvalue << " ] success ! " << std::endl;
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
        LOGI << "[ HGet " << key << " " << hkey << " ] network error" << std::endl;
        _con_pool->ReturnRedisConnection(std::move(context));
        return "";
    }

    if (reply->type == REDIS_REPLY_NIL) {
        freeReplyObject(reply);
        LOGI << "[ HGet " << key << " " << hkey << " ] key not found" << std::endl;
        _con_pool->ReturnRedisConnection(std::move(context));
        return "";
    }

    std::string value = reply->str;
    freeReplyObject(reply);
    LOGI << "Execut command [ HGet " << key << " " << hkey << " ] success ! " << std::endl;
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
    if (reply == nullptr) {
        LOGI << "Execut command [ Del " << key <<  " ] failure ! " << std::endl;
        // freeReplyObject(reply);
        _con_pool->ReturnRedisConnection(std::move(context));
        return false;
    }
    if (reply->type != REDIS_REPLY_INTEGER) {
        LOGI << "Execut command [ Del " << key <<  " ] failure ! " << std::endl;
        freeReplyObject(reply);
        _con_pool->ReturnRedisConnection(std::move(context));
        return false;
    }
    //这样调用者能区分"删了 1 个"和"key 不存在"。
    bool deleted = (reply->integer > 0);
    LOGI << "[ DEL " << key << " ] " << (deleted ? "deleted" : "key not found")
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
    if (reply == nullptr) {
        LOGI << "Not Found [ Key " << key << " ]  ! " << std::endl;
        // freeReplyObject(reply);
        _con_pool->ReturnRedisConnection(std::move(context));
        return false;
    }
    if (reply->type != REDIS_REPLY_INTEGER || reply->integer == 0) {
        LOGI << "Not Found [ Key " << key << " ]  ! " << std::endl;
        freeReplyObject(reply);
        _con_pool->ReturnRedisConnection(std::move(context));
        return false;
    }
    bool exists = (reply->integer > 0);
    LOGI << "[ EXISTS " << key << " ] " << (exists ? "found" : "not found") << std::endl;
    freeReplyObject(reply);
    _con_pool->ReturnRedisConnection(std::move(context));
    return exists;
}

//关闭-------------------
// void RedisMgr::Close()
// {
//     _con_pool->Close();
//     _con_pool->ClearConnections();
// }


/*  HDEL LOGIN_COUNT chatserver1
    #      ↑ key      ↑ field

    Hash: LOGIN_COUNT
    ├── chatserver1 → "5"    ← 删除这个字段
    └── chatserver2 → "3"*/
bool RedisMgr::HDel(const std::string& key, const std::string& field)
{
	auto connect = _con_pool->GetRedisConnection();
	if (connect == nullptr) {
		return false;
	}

	Defer defer([&connect, this]() {
		_con_pool->ReturnRedisConnection(std::move(connect));
		});

	redisReply* reply = (redisReply*)redisCommand(connect.get(), "HDEL %s %s", key.c_str(), field.c_str());
	if (reply == nullptr) {
		LOGE << "HDEL command failed" << std::endl;
		return false;
	}

	bool success = false;
	if (reply->type == REDIS_REPLY_INTEGER) {
		success = reply->integer > 0;
	}

	freeReplyObject(reply); //？？？？reply能否使用shared_ptr，这样就不用手动释放了
	return success;
}

bool RedisMgr::HIncrBy(const std::string& key, const std::string& field, int incr){
    auto connect = _con_pool->GetRedisConnection();
    if(connect == nullptr){
        LOGE << "HIncrBy: 获取 Redis 连接失败";
        return false;
    }
    Defer defer([&connect, this](){
        _con_pool->ReturnRedisConnection(std::move(connect));
    });

    //HINCRBY ————> 原子的
    redisReply* reply = (redisReply*)redisCommand(connect.get(), "HINCRBY %s %s %d", key.c_str(), field.c_str(), incr);
    if(reply == nullptr){
        LOGE << "HIncrBy: command failed, key=" << key << " field=" << field;
        return false;
    }
    // HINCRBY 成功一定返回整数（自增后的新值），注意它可能是 0 甚至负数
    bool success = (reply->type == REDIS_REPLY_INTEGER);
    if(success){
        LOGI << "HIncrBy " << key << " " << field << " " << incr
             << " -> " << reply->integer;
    }else{
        LOGE << "HIncrBy: unexpected reply type = " << reply->type;
    }

    freeReplyObject(reply);
    return success;
}

bool RedisMgr::Ping(std::chrono::milliseconds timeout){
    auto connect = _con_pool->GetRedisConnection(timeout);
    if(connect == nullptr){
        LOGE << "Ping: 获取 Redis 连接失败";
        return false;
    }
    auto* reply = static_cast<redisReply*>(redisCommand(connect.get(), "PING"));
    const bool ok = (reply != nullptr && reply->type == REDIS_REPLY_STATUS);
    if(reply)
        freeReplyObject(reply);
    if(ok){
        _con_pool->ReturnRedisConnection(std::move(connect));   // 好连接才还回去
    }
    return ok;
}