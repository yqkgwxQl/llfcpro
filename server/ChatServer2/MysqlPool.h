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

//正常连接+连接时长
class SqlConnection{
public:
    SqlConnection(sql::Connection* con,int64_t lasttime):_con(con),_last_oper_time(lasttime){

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
    std::unique_ptr<SqlConnection> GetSqlConnection();
    void ReturnSqlConnection(std::unique_ptr<SqlConnection>);
    void checkConnection();
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
};