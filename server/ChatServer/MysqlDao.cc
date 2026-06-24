#include "MysqlDao.h"
#include "ConfigMgr.h"
#include "const.h"
#include "data.h"
#include "MysqlPool.h"

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
    try{
        if(con == nullptr){
            return false;
        }
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
            std::cout << "Result: " << result << std::endl;
            _mysqlPool->ReturnSqlConnection(std::move(con));
            return result;
        }else{
            _mysqlPool->ReturnSqlConnection(std::move(con));
            return -1;
        }
    }catch(sql::SQLException& e){
        _mysqlPool->ReturnSqlConnection(std::move(con));
        std::cerr << "SQLException: " << e.what();
        std::cerr << " MySQL error code: " << e.getErrorCode();
        std::cerr << ", SQLState: " << e.getSQLState() << " )" << std::endl;
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
        std::cerr << "SQLException: " << e.what();
        std::cerr << " (MySQL error code: " << e.getErrorCode();
        std::cerr << ", SQLState: " << e.getSQLState() << " )" << std::endl;
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
        std::cout << "Updated rows: " << updateCount << std::endl;
        _mysqlPool->ReturnSqlConnection(std::move(con));
        return true;
    }catch(sql::SQLException& e){
        _mysqlPool->ReturnSqlConnection(std::move(con));
        std::cerr << "SQLException: " << e.what();
        std::cerr << " (MySQL error code: " << e.getErrorCode();
        std::cerr << ", SQLState: " << e.getSQLState() << " )" << std::endl;
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
        std::cerr << "SQLException: " << e.what();
        std::cerr << " (MySQL error code: " << e.getErrorCode();
        std::cerr << ", SQLState: " << e.getSQLState() << " )" << std::endl;
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
        std::cerr << "SQLException: " << e.what();
        std::cerr << " (MySQL error code: " << e.getErrorCode();
        std::cerr << ", SQLState: " << e.getSQLState() << " )" << std::endl;
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
            std::cout << "Password: " << origin_pwd << std::endl;
            break;
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
        std::cerr << "SQLException: " << e.what();
        std::cerr << " (MySQL error code: " << e.getErrorCode();
        std::cerr << ", SQLState: " << e.getSQLState() << " )" << std::endl;
        return false;
    }
}

bool MysqlDao::GetUser(const int uid,UserInfo& userInfo){
    auto con = _mysqlPool->GetSqlConnection();
    if(con == nullptr){
        std::cerr << "MysqlPool::GetSqlConnection() error" << std::endl;
        return false;
    }
    Defer defer([&con,this](){
        _mysqlPool->ReturnSqlConnection(std::move(con));
    });
    try{
        std::unique_ptr<sql::PreparedStatement> pstmt(con->_con->prepareStatement("SELECT * FROM user WHERE uid = ?"));
        pstmt->setInt(1,uid);
        std::unique_ptr<sql::ResultSet> ret(pstmt->executeQuery());
        if(ret->next()){
            userInfo.pwd = ret->getString("pwd");
            userInfo.email = ret->getString("email");
            userInfo.name = ret->getString("name");
            userInfo.uid = ret->getInt("uid");
            userInfo.nick = ret->getString("nick");
			userInfo.desc = ret->getString("desc");
			userInfo.sex = ret->getInt("sex");
			userInfo.icon = ret->getString("icon");
            return true;
        }
        return false;
    }catch(sql::SQLException& e){
        std::cerr << "SQLException: " << e.what();
        std::cerr << " (MySQL error code: " << e.getErrorCode();
        std::cerr << ", SQLState: " << e.getSQLState() << " )" << std::endl;
        return false;
    }
}