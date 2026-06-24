#pragma once
#include "Singleton.h"
#include "hiredis/hiredis.h"
#include "RedisConPool.h"

//hiredis 是 Redis 官方提供的 C 语言客户端库
/*封装了 Redis 的常用命令：
方法	    对应 Redis 命令	    说明
Connect	         —	        连接 Redis 服务器
Auth	      AUTH	        密码认证
Get / Set	  GET / SET	    字符串读写
LPush / LPo   LPUSH / LPOP	列表左进左出
RPush / RPop  RPUSH / RPOP	列表右进右出
HSet / HGet	  HSET / HGET	哈希表操作
Del	          DEL	        删除键
ExistsKey	  EXISTS	    判断键是否存在
Close	        —	        关闭连接*/
class RedisMgr: public Singleton<RedisMgr>, 
    public std::enable_shared_from_this<RedisMgr>
{
    friend class Singleton<RedisMgr>;
public:
    ~RedisMgr();
    // bool Connect(const std::string& host, int port);
    bool Get(const std::string &key, std::string& value);
    bool Set(const std::string &key, const std::string &value);
    // bool Auth(const std::string &password);
    bool LPush(const std::string &key, const std::string &value);
    bool LPop(const std::string &key, std::string& value);
    bool RPush(const std::string& key, const std::string& value);
    bool RPop(const std::string& key, std::string& value);
    bool HSet(const std::string &key, const std::string  &hkey, const std::string &value);
    bool HSet(const char* key, const char* hkey, const char* hvalue, size_t hvaluelen);
    std::string HGet(const std::string &key, const std::string &hkey);
    bool Del(const std::string &key);
    bool ExistsKey(const std::string &key);
    void Close();
private:
    RedisMgr();

    std::unique_ptr<RedisConPool> _con_pool;
    // redisContext* _connect;  //hiredis 的连接上下文，保存与 Redis 服务器的 TCP 连接状态。
    // redisReply* _reply;      //hiredis 的命令回复对象，用于存储每次命令执行的结果。
};