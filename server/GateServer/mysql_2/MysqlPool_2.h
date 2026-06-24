#pragma once
#include <string>
#include <mutex>
#include <memory>
#include <condition_variable>
#include <queue>
#include <thread>
#include <chrono>
#include <mysql.h>
#include <atomic>

struct MysqlDeleter {
    void operator()(MYSQL* con) const {
        if (con) mysql_close(con);
    }
};
using MysqlPtr = std::unique_ptr<MYSQL, MysqlDeleter>;

// 包装 MYSQL* 连接 + 最后操作时间
class SqlConnection_2 {
public:
    SqlConnection_2(MYSQL* con, int64_t lasttime)
        : _con(con), _last_oper_time(lasttime) {}

    MysqlPtr _con;
    int64_t _last_oper_time;
};

class MysqlPool_2 {
public:
    MysqlPool_2(const std::string& host, int port,
                const std::string& user, const std::string& pwd,
                const std::string& schema, int poolSize);
    ~MysqlPool_2();
    void Close();
    void ClearConnections();
    std::unique_ptr<SqlConnection_2> GetSqlConnection();
    void ReturnSqlConnection(std::unique_ptr<SqlConnection_2>);
    void checkConnection();

private:
    std::string host_;
    int port_;
    std::string user_;
    std::string pwd_;
    std::string schema_;
    int poolSize_;
    std::queue<std::unique_ptr<SqlConnection_2>> SqlConnections_;
    std::mutex mutex_;
    std::atomic<bool> b_stop_{false};
    std::condition_variable cond_;
    std::thread check_thread_;
};
