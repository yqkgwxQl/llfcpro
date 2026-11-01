#include "MysqlDao.h"
#include "Logger.h"
#include "ConfigMgr.h"
#include "const.h"

MysqlDao::MysqlDao(){
    auto & cfg = ConfigMgr::GetInstance();
    const auto& host = cfg["Mysql"]["Host"];
    const auto& port = cfg["Mysql"]["Port"];
    const auto& pwd = cfg["Mysql"]["Passwd"];
    const auto& schema = cfg["Mysql"]["Schema"];
    const auto& user = cfg["Mysql"]["User"];
    //host + ":" + port, user, pwd, schema,5
    _mysqlPool.reset(new MysqlPool(host + ":" + port, user, pwd, schema, 5));
}

MysqlDao::~MysqlDao(){
    _mysqlPool->Close();
}

int MysqlDao::RegUser(const std::string& name,const std::string& email,const std::string& pwd){
    auto con = _mysqlPool->GetSqlConnection();
    if(con == nullptr){
        return false;
    }
    Defer defer([this,&con](){
        this->_mysqlPool->ReturnSqlConnection(std::move(con));
    });
    //con这里为什么可以用引用？  &con ————> 本质上还是con(虽然con本身类型为std::unique_ptr)
    //那defer先于con析构，defer析构完：con变为nullptr(std::unique_ptr) but nullptr 的 unique_ptr 析构是空操作,所以最后析构con是没问题的
    try{
        // @result 是 MySQL 的会话变量，用来接收存储过程的输出参数
        //为什么要用 @result 而不是第四个 ？因为 MySQL 的 PreparedStatement 不直接支持注册输出参数，所以用会话变量绕一下。
        std::unique_ptr<sql::PreparedStatement> stmt(con->_con->prepareStatement("CALL reg_user(?,?,?,@result)"));
        stmt->setString(1,name);
        stmt->setString(2,email);
        stmt->setString(3,pwd);
        stmt->execute();

        std::unique_ptr<sql::Statement> stmtResult(con->_con->createStatement());
        std::unique_ptr<sql::ResultSet> res(stmtResult->executeQuery("SELECT @result AS result"));
        if(res->next()){
            int result = res->getInt("result");
            LOGI << "Result: " << result << std::endl;
            return result;
        }else{
            return -1;
        }
    }catch(sql::SQLException& e){
        LOGE << "SQLException: " << e.what() << ", code=" << e.getErrorCode() << ", SQLState=" << e.getSQLState();
        return -1;
    }
}

int MysqlDao::RegUserTransaction(const std::string& name, const std::string& email, const std::string& pwd, const std::string& icon){
    auto con = _mysqlPool->GetSqlConnection();
    if(con == nullptr){
        return -1;
    }
    Defer defer([this,&con](){
        try{
            con->_con->setAutoCommit(true);
        }catch(...){

        }   /*这里Defer是析构函数，这里只起到拦截异常的作用，不处理异常。(而且也不需要处理什么异常，真正的异常在主线已经处理好了)*/
        this->_mysqlPool->ReturnSqlConnection(std::move(con));
    });
    try{
        //1.关闭Mysql连接的自动提交
        con->_con->setAutoCommit(false);
    //关闭自动提交 ——> MySQL 默认 autocommit = 1：每条语句执行完立即提交。关掉它之后，语句就进入一个显式事务，直到你 commit() 或 rollback()
    //防止由于注册失败导致浪费自增的uid     commit() -> 确认并提交 rollback() -> 回滚并撤销

        //2.查询邮箱，用户名是否已存在
        std::unique_ptr<sql::PreparedStatement> pstmt1(con->_con->prepareStatement("SELECT 1 FROM user WHERE email = ?"));   
        pstmt1->setString(1,email);
        std::unique_ptr<sql::ResultSet> ret1(pstmt1->executeQuery());
        if(ret1->next()){
            con->_con->rollback();
            LOGI << "email " << email << " exist" << std::endl;
            return 0;
        }
        
        std::unique_ptr<sql::PreparedStatement> pstmt2(con->_con->prepareStatement("SELECT 1 FROM user WHERE name = ?"));
        pstmt2->setString(1,name);
        std::unique_ptr<sql::ResultSet> ret2(pstmt2->executeQuery());
        if(ret2->next()){
            con->_con->rollback();
            LOGI << "user_name " << name << " exist" << std::endl;
            return 0;
        }

        //3.取出更新过后的用户id
        std::unique_ptr<sql::PreparedStatement> pstmt3(con->_con->prepareStatement("UPDATE user_id SET id = id + 1"));
        pstmt3->executeUpdate();

        std::unique_ptr<sql::PreparedStatement> pstmt4(con->_con->prepareStatement("SELECT id FROM user_id "));
        std::unique_ptr<sql::ResultSet> ret4(pstmt4->executeQuery());
        int newUid = 0;
        if(ret4->next()){
            newUid = ret4->getInt("id");
        }else{
            con->_con->rollback();
            LOGI << "select id from user_id failed" << std::endl;
            return -1;
        }

        //4.正式插入用户信息
        std::unique_ptr<sql::PreparedStatement> pstmt5(con->_con->prepareStatement("INSERT INTO user (uid, name, email, pwd, nick, icon) "
            "VALUES (?,?,?,?,?,?)"));
        pstmt5->setInt(1,newUid);
        pstmt5->setString(2,name);
        pstmt5->setString(3,email);
        pstmt5->setString(4,pwd);
        pstmt5->setString(5,name);
        pstmt5->setString(6,icon);

        int updateLine = pstmt5->executeUpdate();
        if(updateLine <= 0){
            LOGI << "insert new_user_info failed" << std::endl;
            con->_con->rollback();
            return -1;
        }
        con->_con->commit();
        LOGI << "newuser insert into user success" << std::endl;
		return newUid;

    }catch(sql::SQLException& e){
        try{
            con->_con->rollback();
        }catch(sql::SQLException& e2){
            LOGE << "恢复 autocommit 时 rollback 失败（连接可能已断）-----------" << std::endl;
            LOGE << "SQLException: " << e2.what() << ", code=" << e2.getErrorCode() << ", SQLState=" << e2.getSQLState();
        }
        LOGE << "RegUserTransaction 失败: " << e.what() << ", code=" << e.getErrorCode() << ", SQLState=" << e.getSQLState();
        return -1;
    }catch(const std::exception& e){
        try{
            con->_con->rollback();
        }catch(sql::SQLException& e2){
            LOGE << "恢复 autocommit 时 rollback 失败（连接可能已断）-----------" << std::endl;
            LOGE << "SQLException: " << e2.what() << ", code=" << e2.getErrorCode() << ", SQLState=" << e2.getSQLState();
        }
        LOGE << "std:exception " << e.what() << std::endl;
        return -1;
    }
}

bool MysqlDao::CheckEmail(const std::string& name, const std::string& email) {
    auto con = _mysqlPool->GetSqlConnection();
    try{
        if (con == nullptr) {
            return false;
        }
        std::unique_ptr<sql::PreparedStatement> pstmt(con->_con->prepareStatement("SELECT email FROM user WHERE name = ?"));
        //explicit unique_ptr(pointer p) noexcept;        explicit 的含义是：禁止隐式转换，必须显式调用构造函数
        // std::unique_ptr<sql::PreparedStatement> pstmt = std::move(con->_con->prepareStatement("SELECT email FROM user WHERE name = ?"));
        pstmt->setString(1,name);
        /*     execute()	executeQuery()
        返回值	bool	    ResultSet*   
        
        execute() 的返回值含义：
        返回值	 含义
        true	执行产生了一个结果集（ResultSet）
        false	执行产生的是影响行数（INSERT/UPDATE/DELETE）
        SELECT 语句永远返回 true，因为它的"第一结果"总是 ResultSet——哪怕这个 ResultSet 是空的（没有匹配的记录）。 */
        std::unique_ptr<sql::ResultSet> ret(pstmt->executeQuery());
        while(ret->next()){
            if(email != ret->getString("email")){
                _mysqlPool->ReturnSqlConnection(std::move(con));
                return false;
            }else{
                _mysqlPool->ReturnSqlConnection(std::move(con));
                return true;
            }
        }
    }catch (sql::SQLException& e) {
        _mysqlPool->ReturnSqlConnection(std::move(con));
        LOGE << "SQLException: " << e.what() << ", code=" << e.getErrorCode() << ", SQLState=" << e.getSQLState();
        return false;
    }
}

bool MysqlDao::UpdatePwd(const std::string& name,const std::string& pwd){
    auto con = _mysqlPool->GetSqlConnection();
    try{
        if (con == nullptr) {
            return false;
        }
        std::unique_ptr<sql::PreparedStatement> pstmt(con->_con->prepareStatement("UPDATE user SET pwd = ? WHERE name = ?"));
        pstmt->setString(1,pwd);
        pstmt->setString(2,name);
        //executeUpdate() 执行 INSERT、UPDATE、DELETE 语句，并返回受影响的行数。
        int updateCount = pstmt->executeUpdate();
        LOGI << "Updated rows: " << updateCount << std::endl;
        _mysqlPool->ReturnSqlConnection(std::move(con));
        return true;
    }catch(sql::SQLException& e){
        _mysqlPool->ReturnSqlConnection(std::move(con));
        LOGE << "SQLException: " << e.what() << ", code=" << e.getErrorCode() << ", SQLState=" << e.getSQLState();
        return false;
    }
}

// bool MysqlDao::IsExist(const std::string& name,const std::string& email){
//     auto con = _mysqlPool->GetSqlConnection();
//     try{
//         if (con == nullptr) {
//             return false;
//         }
//         std::unique_ptr<sql::PreparedStatement> pstmt1(con->_con->prepareStatement("SELECT 1 FROM user WHERE name = ?"));
//         pstmt1->setString(1,name);
//         std::unique_ptr<sql::ResultSet> ret1(pstmt1->executeQuery());
//         while(ret1->next()){
//             _mysqlPool->ReturnSqlConnection(std::move(con));
//             return true;
//         }

//         std::unique_ptr<sql::PreparedStatement> pstmt2(con->_con->prepareStatement("SELECT 1 FROM user WHERE email = ?"));
//         pstmt2->setString(1,email);
//         std::unique_ptr<sql::ResultSet> ret2(pstmt2->executeQuery());
//         while(ret2->next()){
//             _mysqlPool->ReturnSqlConnection(std::move(con));
//             return true;
//         }
//         _mysqlPool->ReturnSqlConnection(std::move(con));
//         return false;
//     }catch(sql::SQLException& e){
//         _mysqlPool->ReturnSqlConnection(std::move(con));
//         std::cerr << "SQLException: " << e.what();
//         std::cerr << " (MySQL error code: " << e.getErrorCode();
//         std::cerr << ", SQLState: " << e.getSQLState() << " )" << std::endl;
//         return false;
//     }
// }
bool MysqlDao::IsExist(const std::string& name,const std::string& email){
    return IsExist_name(name) && IsExist_email(email);
}

bool MysqlDao::IsExist_name(const std::string& name){
    auto con = _mysqlPool->GetSqlConnection();
    try{
        if (con == nullptr) {
            return false;
        }
        std::unique_ptr<sql::PreparedStatement> pstmt(con->_con->prepareStatement("SELECT 1 FROM user WHERE name = ?"));
        pstmt->setString(1,name);
        std::unique_ptr<sql::ResultSet> ret(pstmt->executeQuery());
        while(ret->next()){
            _mysqlPool->ReturnSqlConnection(std::move(con));
            return true;
        }

        _mysqlPool->ReturnSqlConnection(std::move(con));
        return false;
    }catch(sql::SQLException& e){
        _mysqlPool->ReturnSqlConnection(std::move(con));
        LOGE << "SQLException: " << e.what() << ", code=" << e.getErrorCode() << ", SQLState=" << e.getSQLState();
        return false;
    }
}
bool MysqlDao::IsExist_email(const std::string& email){
    auto con = _mysqlPool->GetSqlConnection();
    try{
        if (con == nullptr) {
            return false;
        }
        std::unique_ptr<sql::PreparedStatement> pstmt(con->_con->prepareStatement("SELECT 1 FROM user WHERE email = ?"));
        pstmt->setString(1,email);
        std::unique_ptr<sql::ResultSet> ret(pstmt->executeQuery());
        while(ret->next()){
            _mysqlPool->ReturnSqlConnection(std::move(con));
            return true;
        }

        _mysqlPool->ReturnSqlConnection(std::move(con));
        return false;
    }catch(sql::SQLException& e){
        _mysqlPool->ReturnSqlConnection(std::move(con));
        LOGE << "SQLException: " << e.what() << ", code=" << e.getErrorCode() << ", SQLState=" << e.getSQLState();
        return false;
    }
}

bool MysqlDao::CheckPwd(const std::string& email,const std::string& pwd,UserInfo& userInfo){
    auto con = _mysqlPool->GetSqlConnection();
    try{
        if(con == nullptr){
            return false;
        }
        Defer defer([this,&con](){
            _mysqlPool->ReturnSqlConnection(std::move(con));
        });

        std::unique_ptr<sql::PreparedStatement> pstmt(con->_con->prepareStatement("SELECT * FROM user WHERE email = ?"));
        pstmt->setString(1,email);

        std::unique_ptr<sql::ResultSet> ret(pstmt->executeQuery());
        std::string origin_pwd = "";
        while(ret->next()){
            origin_pwd = ret->getString("pwd");
             // 输出查询到的密码
            LOGI << "Password: " << origin_pwd << std::endl;
            break;
            //这里mustbreak，要不然会有异常
        }
        if(origin_pwd != pwd){
            return false;
        }
        userInfo.pwd = ret->getString("pwd");
        userInfo.email = ret->getString("email");
        userInfo.name = ret->getString("name");
        userInfo.uid = ret->getInt("uid");
        return true;
    }catch(sql::SQLException& e){
        _mysqlPool->ReturnSqlConnection(std::move(con));
        LOGE << "SQLException: " << e.what() << ", code=" << e.getErrorCode() << ", SQLState=" << e.getSQLState();
        return false;
    }
}


