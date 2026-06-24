#pragma once
#include "MysqlPool.h"

class UserInfo;
class MysqlDao{

public:
    MysqlDao();
    ~MysqlDao();
    int RegUser(const std::string& name,const std::string& email,const std::string& pwd);
    bool CheckEmail(const std::string& name, const std::string& email);
    bool UpdatePwd(const std::string& name,const std::string& pwd);
    bool IsExist(const std::string& name,const std::string& email);
    bool IsExist_name(const std::string& name);
    bool IsExist_email(const std::string& email);
    bool CheckPwd(const std::string& email,const std::string& pwd,UserInfo& userInfo); 
    bool GetUser(const int uid,UserInfo& userInfo);
private:
    std::unique_ptr<MysqlPool> _mysqlPool;
};

