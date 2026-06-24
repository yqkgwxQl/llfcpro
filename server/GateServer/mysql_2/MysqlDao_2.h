#pragma once
#include "MysqlPool_2.h"
#include <memory>

class MysqlDao_2 {
public:
    MysqlDao_2();
    ~MysqlDao_2();
    int RegUser(const std::string& name, const std::string& email, const std::string& pwd);
    bool CheckEmail(const std::string& name, const std::string& email);
    bool UpdatePwd(const std::string& name, const std::string& newpwd);
private:
    std::unique_ptr<MysqlPool_2> _mysqlPool;
};
