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
            std::unique_ptr<sql::Connection> con(driver->connect(url_, user_, pwd_));
            //切换到指定的数据库
            con->setSchema(schema_);
            //获取当前时间
            auto currentTime = std::chrono::system_clock::now().time_since_epoch();
            //将时间转换为秒级时间戳
            long long timestamp = std::chrono::duration_cast<std::chrono::seconds>(currentTime).count();

            SqlConnections_.push(std::make_unique<SqlConnection>(std::move(con),timestamp));
            LOGI << "mysql connection init success" << std::endl;
        }

        check_thread_  = std::thread([this](){
            while(!b_stop_){
                try{
                    checkConnection();
                }catch (const std::exception& e) {
                    LOGE << "checkConnection 异常: " << e.what();
                }catch (...) {
                    LOGE << "checkConnection 未知异常";
                }
                //每 1min 检测一次
                for (int i = 0; i < 60 && !b_stop_; ++i)      // 把 60s 睡改成 1s 粒度
                    std::this_thread::sleep_for(std::chrono::seconds(1));
                //为什么要这样写？Close() 里 join 的实现是对的，但如果线程正睡在这 60 秒里，join 就得等它睡醒。
                }
            });
    }catch(std::exception& e){
        LOGE << "mysql pool init failed, error is " << e.what()<< std::endl;
    }catch(...){
        LOGE << "未知异常";
    }
}
/*try-catch嵌套如何理解？
    异常会沿着调用栈往外找最近的 catch。如果内层 catch 能处理，就在那里处理；如果内层处理不了，异常会继续往外层传播。*/

MysqlPool::~MysqlPool(){
    Close();
    ClearConnections();
}

void MysqlPool::Close(){
    b_stop_ = true;
    cond_.notify_all();
    //检查线程进行等待
    if(check_thread_.joinable())
        check_thread_.join();
}

void MysqlPool::ClearConnections(){
    std::lock_guard<std::mutex> guard_lock(mutex_);
    while(!SqlConnections_.empty()){
        SqlConnections_.pop();
    }
}

// std::unique_ptr<SqlConnection> MysqlPool::GetSqlConnection(){
//     std::unique_lock<std::mutex> unique_lock(mutex_);
//     cond_.wait(unique_lock,[this](){
//         if(b_stop_)
//             return true;
//         return !SqlConnections_.empty();
//     });
//     if(b_stop_)
//         return nullptr;
//     auto con = std::move(SqlConnections_.front());
//     SqlConnections_.pop();
//     return con;
// }

std::unique_ptr<SqlConnection> MysqlPool::GetSqlConnection(std::chrono::milliseconds timeout){
    std::unique_lock<std::mutex> unique_lock(mutex_);
    const bool ready = cond_.wait_for(unique_lock, timeout, [this](){
        if(b_stop_)
            return true;
        return !SqlConnections_.empty();
    });
    if(b_stop_)
        return nullptr;
    if(!ready){
        unique_lock.unlock();   //LOGE 写时也需要锁，防止等待时间过长导致一直持有锁而不进行归还
        LOGE << "Mysql 连接池等待超时 " << timeout.count()
        << "ms（空闲连接=0，请检查 Mysql 是否启动、[Mysql] 地址/密码是否正确）";
        return nullptr;
    }
    auto con = std::move(SqlConnections_.front());
    SqlConnections_.pop();
    borrowed_++;
    return con;
}

void MysqlPool::ReturnSqlConnection(std::unique_ptr<SqlConnection> con){
    //更新con活跃时间
    if (con) {
        con->_last_oper_time = std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
    }

    {
        std::lock_guard<std::mutex> guard_lock(mutex_);
        if(borrowed_ > 0)
            borrowed_--;
        if(b_stop_)
            return;
        SqlConnections_.push(std::move(con));
    }
    cond_.notify_one();
}

//核心原则：锁内搬数据、锁外做 I/O、放回后通知。
void MysqlPool::checkConnection(){
    //保证SqlConnections_数量保持一致，防止由于丢弃坏的连接，导致总连接数不断减少
    EnsureCapacity();
    // 获取互斥锁，保证线程安全，防止其他线程同时操作连接池
    // 锁内：拿走目前sql池里的连接数量
    int snapshot = 0;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        snapshot = SqlConnections_.size();
    }   // ← 锁在这里释放

    //获取当前系统时间戳（秒）
    auto currentTime = std::chrono::system_clock::now().time_since_epoch();
    long long timestamp = std::chrono::duration_cast<std::chrono::seconds>(currentTime).count();

    for(int i = 0; i < snapshot; i++){
        // 锁内：拿取sql连接
        std::unique_ptr<SqlConnection> con;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if(SqlConnections_.empty() || b_stop_ == true)  
                return;  
            con = std::move(SqlConnections_.front());
            SqlConnections_.pop(); 
        }
        Defer defer([this, &con](){
            // 锁内：放回并进行通知
            {
                std::lock_guard<std::mutex> lock(mutex_);
                if (b_stop_) return;                                // 正在关闭，交给析构清理
                SqlConnections_.push(std::move(con));
            }
            cond_.notify_one();   
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
        }catch(const std::exception& e){
            LOGE << "Error keeping connection alive: " << e.what() << std::endl;
            try{
                //恢复逻辑
                sql::mysql::MySQL_Driver* driver = sql::mysql::get_driver_instance();
                // auto newCon = driver->connect(url_,user_,pwd_);  //裸指针是不行的，一旦下面的setSchema()抛异常，会泄露连接的
                std::unique_ptr<sql::Connection> newCon(driver->connect(url_,user_,pwd_));
                newCon->setSchema(schema_); 
                /*② setSchema 抛异常
                        │
                        ├─→ 栈展开（stack unwinding）
                        │      │
                        │      └─ 销毁作用域内的局部对象：newCon
                        │            └─ unique_ptr::~unique_ptr() → delete _M_ptr → 连接被释放 ✅
                        │
                        └─→ 异常继续向上传播（被 catch 接住）*/
                con->_con = std::move(newCon);
                // con->_con.reset(newCon);
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


// MysqlPool.cc
void MysqlPool::DiscardConnection(std::unique_ptr<SqlConnection> bad) {
    {
        std::unique_lock<std::mutex> lock(mutex_);
        if(borrowed_ > 0)
            borrowed_--;    //正在使用的连接-1
    }
    bad.reset();                       // 析构 → delete sql::Connection → 物理连接关闭

    // 同步补一条，尽量保持容量不变
    std::unique_ptr<sql::Connection> fresh;
    try {
        auto* driver = sql::mysql::get_driver_instance();
        fresh.reset(driver->connect(url_, user_, pwd_));
        fresh->setSchema(schema_);
    }catch (const std::exception& e) {
        //这里出异常了，这个旧连接直接通过智能指针抛弃
        LOGE << "补连接失败（池子暂时少一条）: " << e.what();
        return;                        // 交给 EnsureCapacity 下一轮再补
    }

    const auto now = std::chrono::duration_cast<std::chrono::seconds>(
                         std::chrono::system_clock::now().time_since_epoch()).count();
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (b_stop_) 
            return;
        if (SqlConnections_.size() + borrowed_ >= static_cast<std::size_t>(poolSize_)) 
            return; 
        SqlConnections_.push(std::make_unique<SqlConnection>(std::move(fresh), now));

    }
    cond_.notify_one();
}

bool MysqlPool::EnsureCapacity() {
    std::size_t total = 0;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        total = SqlConnections_.size() + borrowed_;
    }
    if (total >= static_cast<std::size_t>(poolSize_)) return true;

    for (std::size_t i = total; i < static_cast<std::size_t>(poolSize_); ++i) {
        std::unique_ptr<sql::Connection> fresh;
        try {
            auto* driver = sql::mysql::get_driver_instance();
            fresh.reset(driver->connect(url_, user_, pwd_));
            fresh->setSchema(schema_);
        } catch (const std::exception& e) {
            LOGE << "补连接失败: " << e.what();
            return false;
        }
        const auto now = std::chrono::duration_cast<std::chrono::seconds>(
                             std::chrono::system_clock::now().time_since_epoch()).count();
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (b_stop_) 
                return false;
            //total 是循环开始前的快照。循环里每建一条要花几十毫秒（TCP + 认证），这段时间里 DiscardConnection 很可能也补了一条 
            //→ 总数超过 poolSize_。
            if (SqlConnections_.size() + borrowed_ >= static_cast<std::size_t>(poolSize_)) {
                return true;                        // 别人已经补满了，这条丢弃
            }
            SqlConnections_.push(std::make_unique<SqlConnection>(std::move(fresh), now));
        }
        cond_.notify_one();
    }
    return true;
}