#pragma once
#include "MysqlPool.h"

class UserInfo;
class ApplyInfo;
class FriendInfo;
class MysqlDao{

public:
    MysqlDao();
    ~MysqlDao();
    int RegUser(const std::string& name,const std::string& email,const std::string& pwd);
    int RegUserTransaction(const std::string& name, const std::string& email, const std::string& pwd, const std::string& icon);
    bool CheckEmail(const std::string& name, const std::string& email);
    bool UpdatePwd(const std::string& name,const std::string& pwd);
    bool IsExist(const std::string& name,const std::string& email);
    bool IsExist_name(const std::string& name);
    bool IsExist_email(const std::string& email);
    bool CheckPwd(const std::string& email,const std::string& pwd,UserInfo& userInfo); 
    bool GetUser(const int uid,UserInfo& userInfo);
    bool Ping(std::chrono::milliseconds timeout);

    bool AddFriendApply(const int& from_uid,const int& to_uid,const std::string& desc,const std::string& bakname);
    bool AddFriendApply(const int& from_uid,const int& to_uid);
    bool GetApplyList(int uid, std::vector<std::shared_ptr<ApplyInfo>>& list, long long before_id, int limit);
    bool GetFriendList(int uid, std::vector<std::shared_ptr<FriendInfo>>& list, long long before_id, int limit);
    bool GetRelation(int self_uid, int other_uid, int& relation);

    bool AuthFriendApply(const int& from_uid, const int& to_uid);
    bool AddFriend(const int& from_uid, const int& to_uid, const std::string& back_name);
private:
    std::unique_ptr<MysqlPool> _mysqlPool;
};

// 主要功能就是它的析构函数的功能—————— if进入catch段中的错误码符合丢弃的条件，就进行丢弃并补充新的连接；其余正常情况返回连接
class SqlConnGuard {
public:
    SqlConnGuard(MysqlPool* pool, std::unique_ptr<SqlConnection> con)
        : pool_(pool), con_(std::move(con)) {}

    ~SqlConnGuard() {
        if (!con_) return;
        if (broken_) pool_->DiscardConnection(std::move(con_));
        else         pool_->ReturnSqlConnection(std::move(con_));
    }

    SqlConnGuard(const SqlConnGuard&) = delete;
    SqlConnGuard& operator=(const SqlConnGuard&) = delete;

    void markBroken() { broken_ = true; }
    SqlConnection* operator->() const { return con_.get(); }
    explicit operator bool() const { return con_ != nullptr; }

private:
    MysqlPool* pool_;
    std::unique_ptr<SqlConnection> con_;
    bool broken_ = false;
};

