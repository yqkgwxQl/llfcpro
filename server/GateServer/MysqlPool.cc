#include "MysqlPool.h"
#include "Logger.h"
#include <chrono>
#include "const.h"

MysqlPool::MysqlPool(const std::string& url,const std::string& user,const std::string& pwd,const std::string& schema,int poolSize)
    :url_(url),user_(user),pwd_(pwd),schema_(schema),poolSize_(poolSize),b_stop_(false){
    try{
        for(int i = 0; i < poolSize; i++){
            //获取 MySQL 驱动的单例实例（MySQL Connector/C++ 的用法）
            sql::mysql::MySQL_Driver* driver = sql::mysql::get_driver_instance();
            //使用 URL、用户名、密码建立物理数据库连接
            auto* con = driver->connect(url_,user_,pwd_);
            //切换到指定的数据库
            con->setSchema(schema_);
            //获取当前时间
            auto currentTime = std::chrono::system_clock::now().time_since_epoch();
            //将时间转换为秒级时间戳
            long long timestamp = std::chrono::duration_cast<std::chrono::seconds>(currentTime).count();

            SqlConnections_.push(std::make_unique<SqlConnection>(con,timestamp));
            LOGI << "mysql connection init success" << std::endl;
        }

        check_thread_  = std::thread([this](){
            while(!b_stop_){
                //每 1min 检测一次
                checkConnection();
                std::this_thread::sleep_for(std::chrono::seconds(60));
            }
        });
        check_thread_.detach();
    }catch(std::exception& e){
        LOGI << "mysql pool init failed, error is " << e.what()<< std::endl;
    }
}

MysqlPool::~MysqlPool(){
    // Close();
    std::lock_guard<std::mutex> guard_lock(mutex_);
    while(!SqlConnections_.empty()){
        SqlConnections_.pop();
    }
}

void MysqlPool::Close(){
    b_stop_ = true;
    cond_.notify_all();
}

void MysqlPool::ClearConnections(){
    std::lock_guard<std::mutex> guard_lock(mutex_);
    while(!SqlConnections_.empty()){
        SqlConnections_.pop();
    }
}

std::unique_ptr<SqlConnection> MysqlPool::GetSqlConnection(){
    std::unique_lock<std::mutex> unique_lock(mutex_);
    cond_.wait(unique_lock,[this](){
        if(b_stop_)
            return true;
        return !SqlConnections_.empty();
    });
    if(b_stop_)
        return nullptr;
    auto con = std::move(SqlConnections_.front());
    SqlConnections_.pop();
    return con;
}

void MysqlPool::ReturnSqlConnection(std::unique_ptr<SqlConnection> con){
    {
        std::lock_guard<std::mutex> guard_lock(mutex_);
        if(b_stop_)
            return;
        SqlConnections_.push(std::move(con));
    }
    cond_.notify_one();
}

void MysqlPool::checkConnection(){
    // 1. 获取互斥锁，保证线程安全，防止其他线程同时操作连接池
    std::lock_guard<std::mutex> guard(mutex_);

    // 2. 记录当前池子大小，避免循环过程中因重连导致的大小变化影响遍历
    int poolsize = SqlConnections_.size();

    // 3. 获取当前系统时间戳（秒）
    auto currentTime = std::chrono::system_clock::now().time_since_epoch();
    long long timestamp = std::chrono::duration_cast<std::chrono::seconds>(currentTime).count();

    // 4. 遍历连接池中的每一个连接
    for(int i = 0; i < poolsize; i++){
        auto con = std::move(SqlConnections_.front());
        SqlConnections_.pop();

        //使用 Defer 机制（RAII），确保无论后续发生什么（包括异常），连接最终都会被放回池中
        Defer defer([&con,this]()->void{
            SqlConnections_.push(std::move(con));
        });

        //假如那个连接的最新操作时间还没有超过10min，那么不进行检测
        if(timestamp - con->_last_oper_time < 600)
            continue;
        
        //执行必要的查询逻辑
        try{
            //创建 Statement 对象
            std::unique_ptr<sql::Statement> stmt(con->_con->createStatement());
            // auto stmt = std::make_unique<sql::Statement>(con->_con->createStatement());
            //执行简单的查询 “SELECT 1“ 来验证是否存活
            stmt->executeQuery("SELECT 1");

            con->_last_oper_time = timestamp;
            LOGI << "execute timer alive query , cur is " << timestamp << std::endl;
        }catch(std::exception& e){
            LOGI << "Error keeping connection alive: " << e.what() << std::endl;
            try{
                //恢复逻辑
                sql::mysql::MySQL_Driver* driver = sql::mysql::get_driver_instance();
                auto* newCon = driver->connect(url_,user_,pwd_);
                newCon->setSchema(schema_);
                con->_con.reset(newCon);
                con->_last_oper_time = timestamp;
                LOGI << "mysql again connection success" << std::endl;
            }catch(std::exception& reconnectEx){
                // 重连失败，连接保持原样，下次再试
                LOGE << "Reconnect failed:  will attach next time" << reconnectEx.what() << std::endl;
            }
        }
    }
}

//其实我偏向无Defer的，锁的密度不会很大
// void MySqlPool::checkConnection(){
//     // 获取当前系统时间戳（秒）
//     auto currentTime = std::chrono::system_clock::now().time_since_epoch();
//     long long timestamp = std::chrono::duration_cast<std::chrono::seconds>(currentTime).count();

//     // 临时容器，存放所有连接
//     std::vector<std::shared_ptr<SqlConnection>> connections;

//     // 【阶段一】加锁取出所有连接
//     {
//         std::lock_guard<std::mutex> guard(mutex_);
//         int poolsize = SqlConnections_.size();
//         for(int i = 0; i < poolsize; i++){
//             auto con = std::move(SqlConnections_.front());
//             SqlConnections_.pop();
//             connections.push_back(std::move(con));
//         }
//     } // 锁在这里释放

//     // 【阶段二】在锁外执行检测（不阻塞其他线程）
//     for(auto& con : connections){
//         // 如果距离上次操作不足10分钟，跳过检测
//         if(timestamp - con->_last_oper_time >= 600){
//             try{
//                 std::unique_ptr<sql::Statement> stmt(con->_con->createStatement());
//                 stmt->executeQuery("SELECT 1");
//                 con->_last_oper_time = timestamp;
//             }catch(std::exception& e){
//                 std::cout << "Error keeping connection alive: " << e.what() << std::endl;
//                 try{
//                     sql::mysql::MySQL_Driver* driver = sql::mysql::get_mysql_driver_instance();
//                     auto* newCon = driver->connect(url_, user_, pwd_);
//                     newCon->setSchema(schema_);
//                     con->_con.reset(newCon);
//                     con->_last_oper_time = timestamp;
//                     std::cout << "mysql again connection success" << std::endl;
//                 }catch(std::exception& reconnectEx){
//                     std::cerr << "Reconnect failed: will retry next time. " << reconnectEx.what() << std::endl;
//                 }
//             }
//         }
//     }

//     // 【阶段三】加锁将连接放回池中
//     {
//         std::lock_guard<std::mutex> guard(mutex_);
//         for(auto& con : connections){
//             SqlConnections_.push(std::move(con));
//         }
//     }
// }