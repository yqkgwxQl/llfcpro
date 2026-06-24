#include "MysqlMgr_2.h"

MysqlMgr_2::MysqlMgr_2() {}

MysqlMgr_2::~MysqlMgr_2() {}

int MysqlMgr_2::RegUser(const std::string& name, const std::string& email, const std::string& pwd) {
    return _dao.RegUser(name, email, pwd);
}

bool MysqlMgr_2::CheckEmail(const std::string& name, const std::string& email) {
    return _dao.CheckEmail(name, email);
}

bool MysqlMgr_2::UpdatePwd(const std::string& name, const std::string& pwd) {
    return _dao.UpdatePwd(name, pwd);
}