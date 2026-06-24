#include "LogicSystem.h"
#include "HttpConnection.h"
#include "VarifyGrpcClient.h"
#include "RedisMgr.h"
// #include "MysqlMgr_2.h"
#include "MysqlMgr.h"
#include "const.h"
#include "StatusGrpcClient.h"

//url 和 path ，初始化时，传的是/get_test,调用HandleGet是是通过_get_handlers[path](con);来调用回调函数的，也就是说
// path,即_request.target() == /get_test??????????????????
void LogicSystem::RegGet(std::string url, HttpHandler handler){
    _get_handlers.insert(std::make_pair(url,handler));
}

void LogicSystem::RegPost(std::string url, HttpHandler handler){
    _post_handlers.insert(std::make_pair(url,handler));
}


LogicSystem::LogicSystem(){
    //get注册的url时 /get_test，再传一个回调函数
    RegGet("/get_test",[](std::shared_ptr<HttpConnection> connection){
		connection->_response.set(http::field::content_type, "text/plain");
        beast::ostream(connection->_response.body()) << "receiver get_test req";
        int i = 0;
        for (auto& elem : connection->_get_params) {
            i++;
            beast::ostream(connection->_response.body()) << "param" << i << " key is " << elem.first;
            beast::ostream(connection->_response.body()) << ", " <<  " value is " << elem.second << std::endl;
        }
        //为什么map中key2在前，key1在后？
        /*"key1" 和 "key2" 是非常相似的字符串。
        在简单的哈希算法下，它们极有可能计算出相同的哈希值（或者映射到了同一个桶索引）。
        插入过程模拟：
        先插入 key1 -> 桶里只有 [key1]。
        再插入 key2 -> 发现桶里已经有东西了（冲突），为了效率 ，直接插到链表最前面 -> 桶里变成 [key2] -> [key1]。
        当你遍历这个桶时，就会先读到 key2，再读到 key1。这就是为什么你会看到类似“后进先出”的现象。
        有可能哈希桶都是不一样的
        if 你想要保证顺序，需要用std::map, 而非std::unorder_map 
        why 你的顺序和up主不一样？
            1.std::unorder_map本身就不保证顺序
            2.std::unorder_map 的顺序也与编译器的顺序有关
            up主用的是msvc , 我用的是mingw 的g++
        */

    });

    // void(std::shared_ptr<HttpConnection>) 实际上是bool(std::shared_ptr<HttpConnection>) ,类型不匹配呀？？？？？？？？？？？？？？
    RegPost("/get_varifycode",[](std::shared_ptr<HttpConnection> connection){
        auto body_str = boost::beast::buffers_to_string(connection->_request.body().data());
        std::cout << "receive body is " << body_str << std::endl;
        connection->_response.set(http::field::content_type,"text/json");
        Json::Value root;
        Json::Reader reader;
        Json::Value src_root;
        bool parse_success = reader.parse(body_str,src_root);
        if(!parse_success){
            std::cout << "Failed to parse JSON data!" << std::endl;
            root["error"] = ErrorCodes::Error_Json;
            std::string jsonstr = root.toStyledString();
            beast::ostream(connection->_response.body()) << jsonstr;
            return true;
        }
        //判断key值是否存在，即src_root中是否有email这个key值
        if (!src_root.isMember("email")) {
			std::cout << "email key not found!" << std::endl;
			root["error"] = ErrorCodes::Error_Json;
			std::string jsonstr = root.toStyledString();
			beast::ostream(connection->_response.body()) << jsonstr;
			return true;
		}
        auto email = src_root["email"].asString();
        GetVarifyRsp rsp = VarifyGrpcClient::GetInstance()->GetVarifyCode(email);
        std::cout << "email is " << email << std::endl;
        root["error"] = rsp.error();
        root["email"] = src_root["email"]; 
        //这里为什么不root["code"] = src_root["code"]; ——————> 验证码不是通过网关服务器发送的，是node.js的VarifyServer发送的网关服务器不用携带验证码
        // root["email"] = email;
        //两者的最终效果一致，只是第一种是直接赋值，第二种有隐式类型的转换
        std::string jsonstr = root.toStyledString();
        beast::ostream(connection->_response.body()) << jsonstr;
        return true;
    });

    RegPost("/user_register",[](std::shared_ptr<HttpConnection> connection){
        auto body_str = boost::beast::buffers_to_string(connection->_request.body().data());
        std::cout << "receive body is " << body_str << std::endl;
        connection->_response.set(http::field::content_type,"text/json");
        Json::Value root;
        Json::Reader reader;
        Json::Value src_root;
        bool parse_success = reader.parse(body_str,src_root);
        if(!parse_success){
            std::cout << "Failed to parse JSON data!" << std::endl;
            root["error"] = ErrorCodes::Error_Json;
            std::string jsonstr = root.toStyledString();
            beast::ostream(connection->_response.body()) << jsonstr;
            return true;
        }

        //判断key值是否存在，即src_root中是否有email这个key值
        if (!src_root.isMember("user") || !src_root.isMember("email") || !src_root.isMember("passwd") || !src_root.isMember("confirm") || !src_root.isMember("varifycode")) {
			std::cout << "exsit certain key not found!" << std::endl;
			root["error"] = ErrorCodes::Error_Json;
			std::string jsonstr = root.toStyledString();
			beast::ostream(connection->_response.body()) << jsonstr;
			return true;
		}

        //先查找redis中email对应的验证码是否合理
        std::string varifyCode;
        //防止用一个邮箱收取验证码，用另一个邮箱进行注册
        bool b_get_varify = RedisMgr::GetInstance()->Get(CODEPREFIX + src_root["email"].asString(),varifyCode);
        if(!b_get_varify){
            std::cout << "get varifyCode failed!" << std::endl;
            root["error"] = ErrorCodes::VarifyExpired;
            std::string jsonstr = root.toStyledString();
            beast::ostream(connection->_response.body()) << jsonstr;
            return true;
        }
        //防止你提供虚假的邮箱的验证码
        if(varifyCode != src_root["varifycode"].asString()){
            std::cout << "varifyCode invalid!" << std::endl;
            root["error"] = ErrorCodes::VarifyCodeErr;
            std::string jsonstr = root.toStyledString();
            beast::ostream(connection->_response.body()) << jsonstr;
            return true;
        }

        auto email = src_root["email"].asString();
        auto name = src_root["user"].asString();
        auto pwd = src_root["passwd"].asString();
        auto confirm = src_root["confirm"].asString();
        if (pwd != confirm) {
            std::cout << "password err " << std::endl;
            root["error"] = ErrorCodes::PasswdErr;
            std::string jsonstr = root.toStyledString();
            beast::ostream(connection->_response.body()) << jsonstr;
            return true;
        }

        //查看数据库中是否有此用户---------------
        // int uid = MysqlMgr_2::GetInstance()->RegUser(name, email, pwd);
        int uid = MysqlMgr::GetInstance()->RegUser(name, email, pwd);
        if (uid == 0 || uid == -1) {
            std::cout << " user or email exist" << std::endl;
            root["error"] = ErrorCodes::UserExist;
            std::string jsonstr = root.toStyledString();
            beast::ostream(connection->_response.body()) << jsonstr;
            return true;
        }

        root["error"] = 0;
        root["email"] = src_root["email"];
        root["user"]= src_root["user"].asString();
        root["uid"]=uid;
        //这里为什么还有回给用户密码和验证码？？？？？？？？？？？？？？？？？？？1.密码会被窃取，为什么不存到服务器里2.返回验证码，confirm有什么用？
        //这里密码和confirm都进行加密了
        root["passwd"] = src_root["passwd"].asString();
        root["confirm"] = src_root["confirm"].asString();
        root["varifycode"] = src_root["varifycode"].asString();
        std::string jsonstr = root.toStyledString();
        beast::ostream(connection->_response.body()) << jsonstr;
        return true;
    });

    RegPost("/reset_pwd",[](std::shared_ptr<HttpConnection> connection){
        auto body_str = boost::beast::buffers_to_string(connection->_request.body().data());
        std::cout << "receive body is " << body_str << std::endl;
        connection->_response.set(http::field::content_type,"text/json");
        Json::Value root;
        Json::Reader reader;
        Json::Value src_root;
        bool parse_success = reader.parse(body_str,src_root);
        if(!parse_success){
            std::cout << "Failed to parse JSON data!" << std::endl;
            root["error"] = ErrorCodes::Error_Json;
            std::string jsonstr = root.toStyledString();
            beast::ostream(connection->_response.body()) << jsonstr;
            return true;
        }

        //判断key值是否存在，即src_root中是否有email这个key值
        if (!src_root.isMember("user") || !src_root.isMember("email") || !src_root.isMember("passwd")  || !src_root.isMember("varifycode")) {
			std::cout << "exsit certain key not found!" << std::endl;
			root["error"] = ErrorCodes::Error_Json;
			std::string jsonstr = root.toStyledString();
			beast::ostream(connection->_response.body()) << jsonstr;
			return true;
		}

        auto email = src_root["email"].asString();
        auto name = src_root["user"].asString();
        auto pwd = src_root["passwd"].asString();

        //先查找redis中email对应的验证码是否合理
        std::string varifyCode;
        //防止用一个邮箱收取验证码，用另一个邮箱进行注册 / 验证码过期
        bool b_get_varify = RedisMgr::GetInstance()->Get(CODEPREFIX + src_root["email"].asString(),varifyCode);
        if(!b_get_varify){
            std::cout << "get varifyCode failed!" << std::endl;
            root["error"] = ErrorCodes::VarifyExpired;
            std::string jsonstr = root.toStyledString();
            beast::ostream(connection->_response.body()) << jsonstr;
            return true;
        }
        //防止你提供虚假的邮箱的验证码
        if(varifyCode != src_root["varifycode"].asString()){
            std::cout << "varifyCode invalid!" << std::endl;
            root["error"] = ErrorCodes::VarifyCodeErr;
            std::string jsonstr = root.toStyledString();
            beast::ostream(connection->_response.body()) << jsonstr;
            return true;
        }

        //查询数据库判断此用户名和email是否已在数据库中 ——> 即是否此用户已存在
        bool b_exist = MysqlMgr::GetInstance()->IsExist(name,email);
        // bool b_exist = MysqlMgr::GetInstance()->IsExist_name(name) && MysqlMgr::GetInstance()->IsExist_email(email);
        if(!b_exist){
            std::cout << "user not exist , you cannot reset pwd!" << std::endl;
            root["error"] = ErrorCodes::UserNotExist;
            std::string jsonstr = root.toStyledString();
            beast::ostream(connection->_response.body()) << jsonstr;
            return true;
        }
        //查询数据库判断用户名和邮箱是否匹配---------------
        bool email_valid = MysqlMgr::GetInstance()->CheckEmail(name,email);
        if(!email_valid){
            std::cout << " user email not match" << std::endl;
            root["error"] = ErrorCodes::EmailNotMatch;
            std::string jsonstr = root.toStyledString();
            beast::ostream(connection->_response.body()) << jsonstr;
            return true;
        }

        // 更新密码为最新密码
        bool b_up = MysqlMgr::GetInstance()->UpdatePwd(name, pwd);
        if (!b_up) {
            std::cout << " update pwd failed" << std::endl;
            root["error"] = ErrorCodes::PasswdUpFailed;
            std::string jsonstr = root.toStyledString();
            beast::ostream(connection->_response.body()) << jsonstr;
            return true;
        }

        std::cout << "succeed to update password" << pwd << std::endl;
        root["error"] = 0;
        root["email"] = src_root["email"];
        root["user"]= src_root["user"].asString();
        root["passwd"] = src_root["passwd"].asString();
        //这里回传pwd?????????????????????????????//
        root["varifycode"] = src_root["varifycode"].asString();
        std::string jsonstr = root.toStyledString();
        beast::ostream(connection->_response.body()) << jsonstr;
        return true;
    });

    RegPost("/user_login",[](std::shared_ptr<HttpConnection> connection){
        auto body_str = boost::beast::buffers_to_string(connection->_request.body().data());
        std::cout << "receive body is " << body_str << std::endl;
        connection->_response.set(http::field::content_type,"text/json");
        Json::Value root;
        Json::Reader reader;
        Json::Value src_root;
        bool parse_success = reader.parse(body_str,src_root);
        if(!parse_success){
            std::cout << "Failed to parse JSON data!" << std::endl;
            root["error"] = ErrorCodes::Error_Json;
            std::string jsonstr = root.toStyledString();
            beast::ostream(connection->_response.body()) << jsonstr;
            return true;
        }

        //判断key值是否存在，即src_root中是否有email这个key值
        if (!src_root.isMember("email") || !src_root.isMember("passwd")) {
			std::cout << "exsit certain key not found!" << std::endl;
			root["error"] = ErrorCodes::Error_Json;
			std::string jsonstr = root.toStyledString();
			beast::ostream(connection->_response.body()) << jsonstr;
			return true;
		}

        auto email = src_root["email"].asString();
        auto pwd = src_root["passwd"].asString();
        UserInfo userInfo;  

        //查询数据库判断此email是否已在数据库中 ——> 即是否此用户已存在
        bool b_exist = MysqlMgr::GetInstance()->IsExist_email(email);
        if(!b_exist){
            std::cout << "email is not exist!" << std::endl;
            root["error"] = ErrorCodes::EmailNotExist;
            std::string jsonstr = root.toStyledString();
            beast::ostream(connection->_response.body()) << jsonstr;
            return true;
        }
        //查询数据库判断密码和邮箱是否匹配---------------
        bool pwd_valid = MysqlMgr::GetInstance()->CheckPwd(email,pwd,userInfo);//userInfo 作为输出型参数，回传给用户
        /*回传：userInfo.pwd = ret->getString("pwd");
                userInfo.email = ret->getString("email");
                userInfo.name = ret->getString("name");
                userInfo.uid = ret->getInt("uid");*/
        if(!pwd_valid){
            std::cout << " email and pwd not match" << std::endl;
            root["error"] = ErrorCodes::PasswdInvalid;
            std::string jsonstr = root.toStyledString();
            beast::ostream(connection->_response.body()) << jsonstr;
            return true;
        }

        //查询StatusServer找到合适的连接
        auto reply = StatusGrpcClient::GetInstance()->GetChatServer(userInfo.uid);
        if (reply.error()) {
            std::cout << " grpc get chat server failed, error is " << reply.error()<< std::endl;
            root["error"] = ErrorCodes::RPCStatusFailed;
            std::string jsonstr = root.toStyledString();
            beast::ostream(connection->_response.body()) << jsonstr;
            return true;
        }

        std::cout << "succeed to load userinfo uid is " << userInfo.uid << std::endl;
        root["error"] = 0;
        root["user"] = userInfo.name;
        root["uid"] = userInfo.uid;
        root["token"] = reply.token();
        root["host"] = reply.host();
        root["port"] = reply.port();
        std::string jsonstr = root.toStyledString();
        beast::ostream(connection->_response.body()) << jsonstr;
        return true;
    });
}

LogicSystem::~LogicSystem(){

}

//这里只是调用一下get的回调函数而已
bool LogicSystem::HandleGet(std::string path, std::shared_ptr<HttpConnection> con){
    if(_get_handlers.find(path) == _get_handlers.end()){
        return false;
    }
    //执行回调，传con作为参数
    _get_handlers[path](con);
    return true;
}

bool LogicSystem::HandlePost(std::string path, std::shared_ptr<HttpConnection> con){
    if(_post_handlers.find(path) == _post_handlers.end()){
        return false;
    }
    //执行回调，传con作为参数
    _post_handlers[path](con);
    return true;
}


