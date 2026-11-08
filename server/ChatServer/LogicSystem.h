#pragma once

#include "Singleton.h"
#include "const.h"
#include <functional>
#include <queue>
#include <thread>
#include <memory>
#include <mutex>
#include <condition_variable>

class UserInfo;
class ApplyInfo;
class FriendInfo;
class CSession;
class LogicNode;
class RecvNode;
class Defer;
// using FunCallBack = std::function<void(std::shared_ptr<CSession>,const short& msg_id,const std::string& msg_data)>;
using FunCallBack = std::function<void(std::shared_ptr<CSession>, const std::string& msg_data)>;

class LogicSystem: public Singleton<LogicSystem>{

    friend class Singleton<LogicSystem>;
public:
    ~LogicSystem();
    void PostMsgToQue(std::shared_ptr<LogicNode> logic_node);
    void DealMsg();
    bool GetBaseInfo(std::string base_key, int uid, std::shared_ptr<UserInfo> &userinfo);
    // bool GetFriendApplyInfoList(int uid, std::vector<std::shared_ptr<ApplyInfo>>& list);
    // bool GetHasFriendInfoList(int uid, std::vector<std::shared_ptr<FriendInfo>>& list);
private:
    void RejesterCallBack();
    LogicSystem();

    void LoginHandler(std::shared_ptr<CSession> session, const std::string& msg_data);
    void SearchUserHandler(std::shared_ptr<CSession> session, const std::string& msg_data);
    void GetApplyListHandler(std::shared_ptr<CSession> session, const std::string& msg_data);
    void GetFriendListHandler(std::shared_ptr<CSession> session, const std::string& msg_data);
    void AddFriendApply(std::shared_ptr<CSession> session, const std::string& msg_data);
    void AuthFriendApply(std::shared_ptr<CSession> session, const std::string& msg_data);
    void DealChatTextMsg(std::shared_ptr<CSession> session, const std::string& msg_data);
    
    bool _b_stop;
    std::thread _work_thread;
    //logic_request_queues ————> 逻辑请求队列 这样写更好
    std::queue<std::shared_ptr<LogicNode>> _logic_queues;
    std::mutex _mutex;
    std::condition_variable _cond;
    std::map<short,FunCallBack> _callBacks;
};