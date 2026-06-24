#include "MysqlMgr.h"
#include "data.h"

MysqlMgr::MysqlMgr() {
    
}

MysqlMgr::~MysqlMgr() {

}

int MysqlMgr::RegUser(const std::string& name, const std::string& email, const std::string& pwd){
    return _dao.RegUser(name, email, pwd);
}

bool MysqlMgr::CheckEmail(const std::string& name, const std::string& email){
    return _dao.CheckEmail(name, email);
}

bool MysqlMgr::UpdatePwd(const std::string& name,const std::string& pwd){
    return _dao.UpdatePwd(name,pwd);
}

bool MysqlMgr::IsExist(const std::string& name,const std::string& email){
    return _dao.IsExist(name,email);
}

bool MysqlMgr::IsExist_name(const std::string& name){
    return _dao.IsExist_name(name);
}

bool MysqlMgr::IsExist_email(const std::string& email){
    return _dao.IsExist_email(email);
}

bool MysqlMgr::CheckPwd(const std::string& email,const std::string& pwd,UserInfo& userInfo){
    return _dao.CheckPwd(email,pwd,userInfo);
}
bool MysqlMgr::GetUser(const int uid,UserInfo& userInfo){
    return _dao.GetUser(uid,userInfo);
}