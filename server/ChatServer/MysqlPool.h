#pragma once
#include <string>
#include <mutex>
#include <memory>
#include <condition_variable>
#include <queue>
#include <thread>
#include <jdbc/mysql_driver.h>
#include <jdbc/mysql_connection.h>
#include <jdbc/cppconn/prepared_statement.h>
#include <jdbc/cppconn/resultset.h>
#include <jdbc/cppconn/statement.h>
#include <jdbc/cppconn/exception.h>
#include <atomic>
#include <chrono>

//正常连接+连接时长
class SqlConnection{
public:
    SqlConnection(std::unique_ptr<sql::Connection> con,int64_t lasttime):_con(std::move(con)),_last_oper_time(lasttime){

    }
    std::unique_ptr<sql::Connection> _con;
    int64_t _last_oper_time;  //上次操作时间
};

class MysqlPool{

public:
    MysqlPool(const std::string& url,const std::string& user,const std::string& pwd,const std::string& schema,int poolSize);
    ~MysqlPool();
    void Close();
    void ClearConnections();
    // std::unique_ptr<SqlConnection> GetSqlConnection();
    //添加超时检查
    std::unique_ptr<SqlConnection> GetSqlConnection(std::chrono::milliseconds timeout = std::chrono::seconds(3));
    void ReturnSqlConnection(std::unique_ptr<SqlConnection>);
    void checkConnection();
    //如果只在出错时把连接扔掉，池子会 5 → 4 → 3 慢慢缩水，因为 checkConnection 只重连“池里已有的连接”，从不增加数量。 所以应该丢弃坏连接 + 补一条新连接。
    void DiscardConnection(std::unique_ptr<SqlConnection> bad);   // 丢弃坏连接
    bool EnsureCapacity();                                        // 把池子补到 poolSize_
private:
    std::string url_;  //mysql url
    std::string user_;  
    std::string pwd_;  
    std::string schema_; //数据库
    int poolSize_;      
    std::queue<std::unique_ptr<SqlConnection>> SqlConnections_;
    std::mutex mutex_;
    std::atomic<bool> b_stop_;
    std::condition_variable cond_;
    std::thread check_thread_;
    std::atomic<int> fail_count;
    std::size_t borrowed_ = 0;      // 借出中的连接数，和 SqlConnections_ 共用 mutex_
};