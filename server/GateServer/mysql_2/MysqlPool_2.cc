#include "MysqlPool_2.h"
#include <iostream>

// 辅助：创建一条 MySQL 连接
static MYSQL* CreateMysqlConnection(const std::string& host, int port,
                                     const std::string& user, const std::string& pwd,
                                     const std::string& schema) {
    MYSQL* con = mysql_init(nullptr);
    if (con == nullptr) {
        std::cerr << "mysql_init failed" << std::endl;
        return nullptr;
    }

    if (mysql_real_connect(con, host.c_str(), user.c_str(), pwd.c_str(),
                           schema.c_str(), port, nullptr, 0) == nullptr) {
        std::cerr << "mysql_real_connect failed: " << mysql_error(con) << std::endl;
        mysql_close(con);
        return nullptr;
    }

    return con;
}

MysqlPool_2::MysqlPool_2(const std::string& host, int port,
                          const std::string& user, const std::string& pwd,
                          const std::string& schema, int poolSize)
    : host_(host), port_(port), user_(user), pwd_(pwd), schema_(schema), poolSize_(poolSize) {

    for (int i = 0; i < poolSize_; i++) {
        MYSQL* con = CreateMysqlConnection(host_, port_, user_, pwd_, schema_);
        if (con == nullptr) {
            continue;
        }

        auto now = std::chrono::system_clock::now().time_since_epoch();
        long long ts = std::chrono::duration_cast<std::chrono::seconds>(now).count();
        SqlConnections_.push(std::make_unique<SqlConnection_2>(con, ts));
        std::cout << "mysql connection init success [" << (i + 1) << "/" << poolSize_ << "]" << std::endl;
    }

    check_thread_ = std::thread([this]() {
        while (!b_stop_) {
            checkConnection();
            std::this_thread::sleep_for(std::chrono::seconds(60));
        }
    });
    check_thread_.detach();
}

MysqlPool_2::~MysqlPool_2() {
    Close();
    std::lock_guard<std::mutex> guard_lock(mutex_);
    while (!SqlConnections_.empty()) {
        SqlConnections_.pop();
    }
}

void MysqlPool_2::Close() {
    b_stop_ = true;
    cond_.notify_all();
}

void MysqlPool_2::ClearConnections() {
    std::lock_guard<std::mutex> guard_lock(mutex_);
    while (!SqlConnections_.empty()) {
        SqlConnections_.pop();
    }
}

std::unique_ptr<SqlConnection_2> MysqlPool_2::GetSqlConnection() {
    std::unique_lock<std::mutex> unique_lock(mutex_);
    cond_.wait(unique_lock, [this]() {
        if (b_stop_) return true;
        return !SqlConnections_.empty();
    });
    if (b_stop_) return nullptr;

    auto con = std::move(SqlConnections_.front());
    SqlConnections_.pop();
    return con;
}

void MysqlPool_2::ReturnSqlConnection(std::unique_ptr<SqlConnection_2> con) {
    {
        std::lock_guard<std::mutex> guard_lock(mutex_);
        if (b_stop_) return;
        SqlConnections_.push(std::move(con));
    }
    cond_.notify_one();
}

void MysqlPool_2::checkConnection() {
    std::lock_guard<std::mutex> guard(mutex_);

    int poolsize = SqlConnections_.size();
    auto now = std::chrono::system_clock::now().time_since_epoch();
    long long timestamp = std::chrono::duration_cast<std::chrono::seconds>(now).count();

    for (int i = 0; i < poolsize; i++) {
        auto con = std::move(SqlConnections_.front());
        SqlConnections_.pop();

        // RAII 确保放回
        struct Defer {
            std::unique_ptr<SqlConnection_2>& conn;
            std::queue<std::unique_ptr<SqlConnection_2>>& queue;
            ~Defer() { queue.push(std::move(conn)); }
        } defer{con, SqlConnections_};

        // 如果距离上次操作不足 10 分钟，跳过
        if (timestamp - con->_last_oper_time < 600)
            continue;

        // 执行 SELECT 1 验证连接存活
        if (mysql_query(con->_con.get(), "SELECT 1") != 0) {
            std::cerr << "Connection lost, error: " << mysql_error(con->_con.get()) << std::endl;

            // 重连
            MYSQL* newCon = CreateMysqlConnection(host_, port_, user_, pwd_, schema_);
            if (newCon != nullptr) {
                con->_con.reset(newCon);
                con->_last_oper_time = timestamp;
                std::cout << "mysql reconnection success" << std::endl;
            } else {
                std::cerr << "mysql reconnection failed, will retry next time" << std::endl;
            }
        } else {
            con->_last_oper_time = timestamp;
        }
    }
}
