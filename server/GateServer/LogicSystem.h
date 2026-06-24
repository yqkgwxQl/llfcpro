#pragma once

#include "Singleton.h"
#include "const.h"
#include <functional>
#include <map>

class HttpConnection;
using HttpHandler = std::function<void(std::shared_ptr<HttpConnection>)>;
class LogicSystem: public Singleton<LogicSystem>{

    friend class Singleton<LogicSystem>;
public:
    ~LogicSystem();
    bool HandleGet(std::string url, std::shared_ptr<HttpConnection> connection);
    bool HandlePost(std::string url, std::shared_ptr<HttpConnection> connection);
    //这里的注册只管往对应的map里加上 url 和 handler
    void RegGet(std::string url, HttpHandler handler);
    void RegPost(std::string url, HttpHandler handler);
private:
    LogicSystem();
    std::map<std::string, HttpHandler> _post_handlers;
    std::map<std::string, HttpHandler> _get_handlers;
};