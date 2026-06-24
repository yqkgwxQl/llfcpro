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
class CSession;
class LogicNode;
class RecvNode;
// using FunCallBack = std::function<void(std::shared_ptr<CSession>,const short& msg_id,const std::string& msg_data)>;
using FunCallBack = std::function<void(std::shared_ptr<CSession>,const std::string& msg_data)>;

class LogicSystem: public Singleton<LogicSystem>{

    friend class Singleton<LogicSystem>;
public:
    ~LogicSystem();
    void PostMsgToQue(std::shared_ptr<LogicNode> logic_node);
    void DealMsg();
    bool GetBaseInfo(std::string base_key, int uid, std::shared_ptr<UserInfo> &userinfo);
private:
    void RejesterCallBack();
    LogicSystem();
    void LoginHandler(std::shared_ptr<CSession>,const std::string& msg_data);
    bool _b_stop;
    std::thread _work_thread;
    //logic_request_queues ————> 逻辑请求队列 这样写更好
    std::queue<std::shared_ptr<LogicNode>> _logic_queues;
    std::mutex _mutex;
    std::condition_variable _cond;
    std::map<short,FunCallBack> _callBacks;
};