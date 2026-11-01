#include "MysqlMgr.h"
#include "data.h"

MysqlMgr::MysqlMgr() {
    
}

MysqlMgr::~MysqlMgr() {

}

int MysqlMgr::RegUser(const std::string& name, const std::string& email, const std::string& pwd, const std::string& icon){
    return _dao.RegUserTransaction(name, email, pwd, icon);
}

bool MysqlMgr::CheckEmail(const std::string& name, const std::string& email){
    return _dao.CheckEmail(name, email);
}

bool MysqlMgr::UpdatePwd(const std::string& name,const std::string& pwd){
    return _dao.UpdatePwd(name, pwd);
}

bool MysqlMgr::IsExist(const std::string& name,const std::string& email){
    return _dao.IsExist(name, email);
}

bool MysqlMgr::IsExist_name(const std::string& name){
    return _dao.IsExist_name(name);
}

bool MysqlMgr::IsExist_email(const std::string& email){
    return _dao.IsExist_email(email);
}

bool MysqlMgr::CheckPwd(const std::string& email,const std::string& pwd,UserInfo& userInfo){
    return _dao.CheckPwd(email, pwd, userInfo);
}

bool MysqlMgr::GetUser(const int uid,UserInfo& userInfo){
    return _dao.GetUser(uid, userInfo);
}

bool MysqlMgr::AddFriendApply(const int& from_uid,const int& to_uid,const std::string& desc,const std::string& bakname){
    return _dao.AddFriendApply(from_uid, to_uid, desc, bakname);
}

bool MysqlMgr::AddFriendApply(const int& from_uid,const int& to_uid){
    return _dao.AddFriendApply(from_uid, to_uid);
}

bool MysqlMgr::GetApplyList(int uid, std::vector<std::shared_ptr<ApplyInfo>>& list, long long before_id, int limit){
    return _dao.GetApplyList(uid, list, before_id, limit);
}

bool MysqlMgr::GetFriendList(int uid, std::vector<std::shared_ptr<FriendInfo>>& list, long long before_id, int limit){
    return _dao.GetFriendList(uid, list, before_id, limit);
}

bool MysqlMgr::GetRelation(int self_uid, int other_uid, int& relation){
    return _dao.GetRelation(self_uid, other_uid, relation);
}
bool MysqlMgr::AuthFriendApply(const int& from_uid, const int& to_uid){
    return _dao.AuthFriendApply(from_uid, to_uid);
}

bool MysqlMgr::AddFriend(const int& from_uid, const int& to_uid, const std::string& back_name){
    return _dao.AddFriend(from_uid, to_uid, back_name);
}