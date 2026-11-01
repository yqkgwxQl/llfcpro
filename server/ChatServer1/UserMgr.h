#pragma once
#include <string>
#include "Singleton.h"
#include <unordered_map>

// struct UserInfo{
//     std::string name;
//     std::string pwd;
//     std::string email;
//     std::string token;
//     std::size_t uid;
// };

//因为我们用户登录后，要将连接(session)和用户uid绑定。为以后登陆踢人做准备。所以新增UserMgr管理类.
class CSession;
class UserMgr : public Singleton<UserMgr>
{
    friend class Singleton<UserMgr>;
public:
    ~UserMgr();
    std::shared_ptr<CSession> GetSession(int uid);
    void SetUserSession(std::size_t uid, std::shared_ptr<CSession> session);
    bool RmvUserSession(std::size_t uid, const std::string& session_id);

private:
    UserMgr();
    std::mutex _session_mtx;
    std::unordered_map<std::size_t, std::shared_ptr<CSession>> _uid_to_session;
    /*_uid_to_session
        ├── 1001 → session(用户A的连接)
        ├── 1002 → session(用户B的连接)
        └── 1003 → nullptr （被移除了）*/
};



