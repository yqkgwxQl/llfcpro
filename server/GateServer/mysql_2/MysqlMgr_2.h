#pragma once
#include "Singleton.h"
#include "MysqlDao_2.h"

class MysqlMgr_2 : public Singleton<MysqlMgr_2> {
    friend class Singleton<MysqlMgr_2>;
public:
    ~MysqlMgr_2();
    int RegUser(const std::string& name, const std::string& email, const std::string& pwd);
    bool CheckEmail(const std::string& name, const std::string& email);
    bool UpdatePwd(const std::string& name, const std::string& pwd);
private:
    MysqlMgr_2();
    MysqlDao_2 _dao;
};
