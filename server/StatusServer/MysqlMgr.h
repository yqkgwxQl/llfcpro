#pragma once
#include "Singleton.h"
#include "MysqlDao.h"

class UserInfo;
class ApplyInfo;
class FriendInfo;
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
    bool GetUser(const int uid,UserInfo& userInfo);
    bool Ping(std::chrono::milliseconds timeout = std::chrono::seconds(3));

    bool AddFriendApply(const int& from_uid,const int& to_uid,const std::string& desc,const std::string& bakname);
    bool AddFriendApply(const int& from_uid,const int& to_uid);
    bool GetApplyList(int uid, std::vector<std::shared_ptr<ApplyInfo>>& list, long long before_id, int limit);
    bool GetFriendList(int uid, std::vector<std::shared_ptr<FriendInfo>>& list, long long before_id, int limit);
    bool GetRelation(int self_uid, int other_uid, int& relation);

    bool AuthFriendApply(const int& from_uid, const int& to_uid);
    bool AddFriend(const int& from_uid, const int& to_uid, const std::string& back_name);
private:
    MysqlMgr();
    MysqlDao  _dao;
};