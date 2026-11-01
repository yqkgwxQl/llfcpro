#include "UserMgr.h"
#include "Logger.h"
#include "CSession.h"
#include "CServer.h"

UserMgr::UserMgr(){
    
}

UserMgr::~UserMgr(){
   _uid_to_session.clear();
}

std::shared_ptr<CSession> UserMgr::GetSession(int uid){
    auto iter = _uid_to_session.find(uid);
    if(iter == _uid_to_session.end())
        return nullptr;
    return iter->second;
}

void UserMgr::SetUserSession(std::size_t uid, std::shared_ptr<CSession> session){
    std::shared_ptr<CSession> old_session;
    {
        std::lock_guard<std::mutex> guard_lock(_session_mtx);
        auto iter = _uid_to_session.find(uid);
        if(iter != _uid_to_session.end())
            old_session  = _uid_to_session[uid];
        _uid_to_session[uid] = session;
    }
    if(old_session && old_session != session){
        LOGE << "uid(" << uid << ") 重复登录，踢掉旧 session: " << old_session->GetSessionId();
        old_session->GetServer()->ClearSession(old_session->GetSessionId());
        //这里应该关闭old_session的socket，但是由于多线程受限，不能直接在这里关闭
    }
    return;
}

//说实话，我感觉这样写的好别扭？？？？？？？？？
bool UserMgr::RmvUserSession(std::size_t uid, const std::string& session_id){
    std::lock_guard<std::mutex> guard_lock(_session_mtx);
    auto iter = _uid_to_session.find(uid);
    if(iter == _uid_to_session.end()){
        LOGE << "未能找到uid(" << uid << ")对应的session";
        return false;
    }
    if(iter->second && iter->second->GetSessionId() != session_id){
        LOGI << "uid(" << uid << ") 当前 session 已是 " << iter->second->GetSessionId()
            << "，忽略旧连接 " << session_id << " 的清理";
        return false;                                       // ★ 不删别人的映射
    }
    _uid_to_session.erase(iter);
    return true;                 
}
