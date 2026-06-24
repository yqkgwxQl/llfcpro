#include "UserMgr.h"

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
    auto iter = _uid_to_session.find(uid);
    if(iter != _uid_to_session.end())
        return;
    std::lock_guard<std::mutex> guard_lock(_session_mtx);
    _uid_to_session.insert(std::pair(uid,session));
    return;
}

void UserMgr::RmvUserSession(std::size_t uid){
    auto iter = _uid_to_session.find(uid);
    if(iter == _uid_to_session.end())
        return;
    std::lock_guard<std::mutex> guard_lock(_session_mtx);
    _uid_to_session.erase(uid);
    return;
}
