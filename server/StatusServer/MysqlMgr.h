#pragma once
#include "Singleton.h"
#include "MysqlDao.h"
class MysqlMgr: public Singleton<MysqlMgr>
{
    friend class Singleton<MysqlMgr>;
public:
    ~MysqlMgr();
    int RegUser(const std::string& name, const std::string& email,  const std::string& pwd, const std::string& icon);
    bool CheckEmail(const std::string& name, const std::string& email);
    bool UpdatePwd(const std::string& name,const std::string& pwd);
    bool IsExist(const std::string& name,const std::string& email);
    bool IsExist_name(const std::string& name);
    bool IsExist_email(const std::string& email);
    bool CheckPwd(const std::string& email,const std::string& pwd,UserInfo& userInfo);
private:
    MysqlMgr();
    MysqlDao  _dao;
};