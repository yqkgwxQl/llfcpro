#include "MysqlDao.h"
#include "Logger.h"
#include "ConfigMgr.h"
#include "const.h"
#include "data.h"
#include "MysqlPool.h"

namespace {  
    bool IsConnectionLost(int code) {
        // 2006 CR_SERVER_GONE_ERROR / 2013 CR_SERVER_LOST / 2055 CR_SERVER_LOST_EXTENDED
        return code == 2006 || code == 2013 || code == 2055;
    }
}
/*sql::SQLException的错误码         含义                          处理
2006 CR_SERVER_GONE_ERROR        服务端已关闭连接           连接废了 → 丢弃
2013 CR_SERVER_LOST              查询中途断开               连接废了 → 丢弃
2055 CR_SERVER_LOST_EXTENDED     同上，带扩展信息           丢弃
1062 ER_DUP_ENTRY                唯一键冲突                 连接是好的，照常归还
1146 ER_NO_SUCH_TABLE            表不存在                   连接是好的，照常归还
语法错误 / 约束错误              —                          连接是好的，照常归还*/

MysqlDao::MysqlDao(){
    auto & cfg = ConfigMgr::GetInstance();
    const auto& host = cfg["Mysql"]["Host"];
    const auto& port = cfg["Mysql"]["Port"];
    const auto& pwd = cfg["Mysql"]["Passwd"];
    const auto& schema = cfg["Mysql"]["Schema"];
    const auto& user = cfg["Mysql"]["User"];
    //host + ":" + port, user, pwd, schema,5
    _mysqlPool.reset(new MysqlPool(host + ":" + port, user, pwd, schema, 5));
}

/*  析构顺序：
        1. 执行 ~MysqlDao() 的【函数体】        ← Close() 在这里被调用
        2. 逆序销毁 MysqlDao 的成员变量          ← _mysqlPool 在这里才被析构
        3. 销毁基类子对象*/
MysqlDao::~MysqlDao(){
    
}

/*数据库里 reg_user 的真实定义
    OUT `result` INT
  -1  → 发生 SQL 异常（回滚）
   0  → 用户名已存在 或 邮箱已存在（失败）
  >0  → 新用户的 uid（成功）*/
    //我这里 -1: sql异常 -2: std::exception -3: 未知异常 -4: MysqlPool里获取不到连接 0: 用户名/邮箱已存在 >0: 新用户的uid
int MysqlDao::RegUser(const std::string& name,const std::string& email,const std::string& pwd){
    auto con = _mysqlPool->GetSqlConnection();
    if (con == nullptr) {                        
        LOGE << "获取MysqlPool里的sql连接失败";
        return -4;
    }

    SqlConnGuard guard(_mysqlPool.get(), std::move(con));

    try{
        // @result 是 MySQL 的会话变量，用来接收存储过程的输出参数
        //为什么要用 @result 而不是第四个 ？因为 MySQL 的 PreparedStatement 不直接支持注册输出参数，所以用会话变量绕一下。
        std::unique_ptr<sql::PreparedStatement> stmt(guard->_con->prepareStatement("CALL reg_user(?,?,?,@result)"));
        stmt->setString(1,name);
        stmt->setString(2,email);
        stmt->setString(3,pwd);
        stmt->execute();

        std::unique_ptr<sql::Statement> stmtResult(guard->_con->createStatement());
        std::unique_ptr<sql::ResultSet> res(stmtResult->executeQuery("SELECT @result AS result"));
        if(!res->next()){   //这种情况基本不可能
            LOGE << "reg_user 取不到 @result";
            return -5;
        }
        int result = res->getInt("result");  // -1 0 >0 我都传
        LOGI << "reg_user result: " << result;
        return result;
    }catch(const sql::SQLException& e){
        if (IsConnectionLost(e.getErrorCode())) guard.markBroken();
        LOGE << "SQLException: " << e.what()
             << ", code=" << e.getErrorCode()
             << ", SQLState=" << e.getSQLState();
        return -1;
    }catch(const std::exception& e2){
        LOGE << "std::exception: " << e2.what();
        return -2;
    }catch(...){
        LOGE << "未知异常";
        return -3;
    }
}

int MysqlDao::RegUserTransaction(const std::string& name, const std::string& email, const std::string& pwd,const std::string& icon){
    auto con = _mysqlPool->GetSqlConnection();
    if(con == nullptr){
        return -4;
    }
    SqlConnGuard guard(_mysqlPool.get(), std::move(con));     // ① 接管连接

    // ② 兜底：无论从哪条路径退出，都恢复 autocommit
    //    注意：它必须在 guard 之后声明 → 析构时先执行（先恢复状态，再归还/丢弃）
    Defer restoreAutoCommit([&guard](){
        try {
            guard->_con->setAutoCommit(true);
        } catch (...) {
            guard.markBroken();          // 连 setAutoCommit 都失败 → 连接已死
        }
    });
    try{
        //1.关闭Mysql连接的自动提交
        guard->_con->setAutoCommit(false);
    //关闭自动提交 ——> MySQL 默认 autocommit = 1：每条语句执行完立即提交。关掉它之后，语句就进入一个显式事务，直到你 commit() 或 rollback()
    //防止由于注册失败导致浪费自增的uid     commit() -> 确认并提交 rollback() -> 回滚并撤销

        //2.查询邮箱，用户名是否已存在
        std::unique_ptr<sql::PreparedStatement> pstmt1(guard->_con->prepareStatement("SELECT 1 FROM user WHERE email = ?"));   
        pstmt1->setString(1,email);
        std::unique_ptr<sql::ResultSet> ret1(pstmt1->executeQuery());
        if(ret1->next()){
            guard->_con->rollback();
            LOGI << "email " << email << " exist";
            return 0;
        }
        
        std::unique_ptr<sql::PreparedStatement> pstmt2(guard->_con->prepareStatement("SELECT 1 FROM user WHERE name = ?"));
        pstmt2->setString(1,name);
        std::unique_ptr<sql::ResultSet> ret2(pstmt2->executeQuery());
        if(ret2->next()){
            guard->_con->rollback();
            LOGI << "user_name " << name << " exist";
            return 0;
        }

        //3.取出更新过后的用户id
        std::unique_ptr<sql::PreparedStatement> pstmt3(guard->_con->prepareStatement("UPDATE user_id SET id = id + 1"));
        pstmt3->executeUpdate();

        std::unique_ptr<sql::PreparedStatement> pstmt4(guard->_con->prepareStatement("SELECT id FROM user_id "));
        std::unique_ptr<sql::ResultSet> ret4(pstmt4->executeQuery());
        int newUid = 0;
        if(ret4->next()){
            newUid = ret4->getInt("id");
        }else{
            guard->_con->rollback();
            LOGI << "select id from user_id failed";
            return -1;
        }

        //4.正式插入用户信息
        std::unique_ptr<sql::PreparedStatement> pstmt5(guard->_con->prepareStatement("INSERT INTO user (uid, name, email, pwd, nick, icon) "
            "VALUES (?,?,?,?,?,?)"));
        pstmt5->setInt(1,newUid);
        pstmt5->setString(2,name);
        pstmt5->setString(3,email);
        pstmt5->setString(4,pwd);
        pstmt5->setString(5,name);
        pstmt5->setString(6,icon);

        int updateLine = pstmt5->executeUpdate();
        if(updateLine == 0){
            LOGI << "insert new_user_info failed";
            guard->_con->rollback();
            return -1;
        }
        guard->_con->commit();
        LOGI << "newuser insert into user success";
		return newUid;

    }catch(const sql::SQLException& e){
        if (IsConnectionLost(e.getErrorCode())) guard.markBroken();
        try{
            guard->_con->rollback();
        }catch(const sql::SQLException& e2){
            LOGE << "恢复 autocommit 时 rollback 失败（连接可能已断）";
            LOGE << "SQLException: " << e2.what() << ", code=" << e2.getErrorCode() << ", SQLState=" << e2.getSQLState();
        }catch(...){
            LOGE << "rollback()时出现未知错误";
        }
        LOGE << "由于某些SQL原因失败: " << e.what() << ", code=" << e.getErrorCode() << ", SQLState=" << e.getSQLState();
        return -1;
    }catch(const std::exception& e){
        try{
            guard->_con->rollback();
        }catch(const sql::SQLException& e2){
            LOGE << "恢复 autocommit 时 rollback 失败（连接可能已断）";
            LOGE << "SQLException: " << e2.what() << ", code=" << e2.getErrorCode() << ", SQLState=" << e2.getSQLState();
        }catch(...){
            LOGE << "rollback()时出现未知错误";
        }
        LOGE << "std:exception " << e.what();
        return -2;
    }catch(...){
        try{
            guard->_con->rollback();
        }catch(sql::SQLException& e2){
            LOGE << "恢复 autocommit 时 rollback 失败（连接可能已断）";
            LOGE << "SQLException: " << e2.what() << ", code=" << e2.getErrorCode() << ", SQLState=" << e2.getSQLState();
        }catch(...){
            LOGE << "rollback()时出现未知错误";
        }
        LOGE << "未知异常";
        return -3;
    }
}

bool MysqlDao::CheckEmail(const std::string& name, const std::string& email) {
    auto con = _mysqlPool->GetSqlConnection();
    if (con == nullptr) {                        
        LOGE << "获取MysqlPool里的sql连接失败";
        return false;
    }

    SqlConnGuard guard(_mysqlPool.get(), std::move(con));   // 对下面注释内容的总结
    // bool conn_broken = false;                  
    // Defer defer([this, &con, &conn_broken](){
    //     if (!con) return;                      // 已被处理过
    //     if (conn_broken) {
    //         _mysqlPool->DiscardConnection(std::move(con));   // 丢弃 + 补位
    //     } else {
    //         this->_mysqlPool->ReturnSqlConnection(std::move(con));
    //     }
    // });

    try{
        std::unique_ptr<sql::PreparedStatement> pstmt(guard->_con->prepareStatement("SELECT email FROM user WHERE name = ?"));
        //explicit unique_ptr(pointer p) noexcept;        explicit 的含义是：禁止隐式转换，必须显式调用构造函数
        // std::unique_ptr<sql::PreparedStatement> pstmt = std::move(guard->_con->prepareStatement("SELECT email FROM user WHERE name = ?"));
        pstmt->setString(1,name);
        /*     execute()	executeQuery()
        返回值	bool	    ResultSet*   
        
        execute() 的返回值含义：
        返回值	 含义
        true	执行产生了一个结果集（ResultSet）
        false	执行产生的是影响行数（INSERT/UPDATE/DELETE）
        SELECT 语句永远返回 true，因为它的"第一结果"总是 ResultSet——哪怕这个 ResultSet 是空的（没有匹配的记录）。 */
        std::unique_ptr<sql::ResultSet> ret(pstmt->executeQuery());
        //"我知道最多只有一行" → if；"我不知道有几行，都得处理" → while
        if(ret->next()){     //next() 有两个作用：① 判断还有没有数据 ② 把游标挪过去。
            return email == ret->getString("email");
        }else{
            return false;
        }
            
    }catch(const sql::SQLException& e){
        if (IsConnectionLost(e.getErrorCode())) guard.markBroken();
        LOGE << "SQLException: " << e.what()
             << ", code=" << e.getErrorCode()
             << ", SQLState=" << e.getSQLState();
        return false;
    }catch(const std::exception& e2){
        LOGE << "std::exception: " << e2.what();
        return false;
    }catch(...){
        LOGE << "未知异常";
        return false;
    }
}

bool MysqlDao::UpdatePwd(const std::string& name,const std::string& pwd){
    auto con = _mysqlPool->GetSqlConnection();
    if (con == nullptr) {                        
        LOGE << "获取MysqlPool里的sql连接失败";
        return false;
    }

    SqlConnGuard guard(_mysqlPool.get(), std::move(con));

    try{
        std::unique_ptr<sql::PreparedStatement> pstmt(guard->_con->prepareStatement("UPDATE user SET pwd = ? WHERE name = ?"));
        pstmt->setString(1,pwd);
        pstmt->setString(2,name);
        //executeUpdate() 执行 INSERT、UPDATE、DELETE 语句，并返回受影响的行数。
        int updateCount = pstmt->executeUpdate();
        LOGI << "Updated rows: " << updateCount;
        return true;
    }catch(const sql::SQLException& e){
        LOGE << "SQLException: " << e.what()
             << ", code=" << e.getErrorCode()
             << ", SQLState=" << e.getSQLState();
        return false;
    }catch(const std::exception& e2){
        LOGE << "std::exception: " << e2.what();
        return false;
    }catch(...){
        LOGE << "未知异常";
        return false;
    }
}

bool MysqlDao::IsExist(const std::string& name,const std::string& email){
    return IsExist_name(name) || IsExist_email(email);
}

bool MysqlDao::IsExist_name(const std::string& name){
    auto con = _mysqlPool->GetSqlConnection();
    if (con == nullptr) {                        
        LOGE << "获取MysqlPool里的sql连接失败";
        return false;
    }

    SqlConnGuard guard(_mysqlPool.get(), std::move(con));

    try{
        std::unique_ptr<sql::PreparedStatement> pstmt(guard->_con->prepareStatement("SELECT 1 FROM user WHERE name = ?"));
        pstmt->setString(1,name);
        std::unique_ptr<sql::ResultSet> ret(pstmt->executeQuery());
        while(ret->next()){
            return true;
        }

        return false;
    }catch(const sql::SQLException& e){
        if (IsConnectionLost(e.getErrorCode())) guard.markBroken();
        LOGE << "SQLException: " << e.what()
             << ", code=" << e.getErrorCode()
             << ", SQLState=" << e.getSQLState();
        return false;
    }catch(const std::exception& e2){
        LOGE << "std::exception: " << e2.what();
        return false;
    }catch(...){
        LOGE << "未知异常";
        return false;
    }
}
bool MysqlDao::IsExist_email(const std::string& email){
    auto con = _mysqlPool->GetSqlConnection();
    if (con == nullptr) {                        
        LOGE << "获取MysqlPool里的sql连接失败";
        return false;
    }

    SqlConnGuard guard(_mysqlPool.get(), std::move(con));

    try{
        std::unique_ptr<sql::PreparedStatement> pstmt(guard->_con->prepareStatement("SELECT 1 FROM user WHERE email = ?"));
        pstmt->setString(1,email);
        std::unique_ptr<sql::ResultSet> ret(pstmt->executeQuery());
        while(ret->next()){
            return true;
        }

        return false;
    }catch(const sql::SQLException& e){
        if (IsConnectionLost(e.getErrorCode())) guard.markBroken();
        LOGE << "SQLException: " << e.what()
             << ", code=" << e.getErrorCode()
             << ", SQLState=" << e.getSQLState();
        return false;
    }catch(const std::exception& e2){
        LOGE << "std::exception: " << e2.what();
        return false;
    }catch(...){
        LOGE << "未知异常";
        return false;
    }
}

bool MysqlDao::CheckPwd(const std::string& email,const std::string& pwd,UserInfo& userInfo){
    auto con = _mysqlPool->GetSqlConnection();
    if (con == nullptr) {                        
        LOGE << "获取MysqlPool里的sql连接失败";
        return false;
    }

    SqlConnGuard guard(_mysqlPool.get(), std::move(con));

    try{
        std::unique_ptr<sql::PreparedStatement> pstmt(guard->_con->prepareStatement("SELECT * FROM user WHERE email = ?"));
        pstmt->setString(1,email);

        std::unique_ptr<sql::ResultSet> ret(pstmt->executeQuery());
        std::string origin_pwd = "";
        /*正常情况能过（break 不移动游标，仍停在第 1 行）。但有个边界：如果 pwd 是空串、而这个邮箱不存在，
        origin_pwd("") == pwd("") 会通过检查，然后在已经走到末尾的结果集上调 getString() → 抛 Invalid row
        （被 catch 吞掉，最后表现为"密码错误"，排查时会误导）。*/
        // while(ret->next()){
        //     origin_pwd = ret->getString("pwd");
        //     break;
        // }
        // if(origin_pwd != pwd){
        //     LOGE << "邮箱所对应的密码并不正确";
        //     return false;
        // }
        // userInfo.pwd = ret->getString("pwd");
        // userInfo.email = ret->getString("email");
        // userInfo.name = ret->getString("name");
        // userInfo.uid = ret->getInt("uid");
        // return true;
        bool found = false;
        while(ret->next()){
            origin_pwd  = ret->getString("pwd");
            userInfo.pwd   = origin_pwd;
            userInfo.email = ret->getString("email");
            userInfo.name  = ret->getString("name");
            userInfo.uid   = ret->getInt("uid");
            found = true;
            break;
        }
        if(!found){
            LOGE << "邮箱不存在";
            return false;
         }
        if(origin_pwd != pwd){ 
            LOGE << "密码不正确"; 
            return false; 
        }
        return true;
    }catch(const sql::SQLException& e){
        if (IsConnectionLost(e.getErrorCode())) guard.markBroken();
        LOGE << "SQLException: " << e.what()
             << ", code=" << e.getErrorCode()
             << ", SQLState=" << e.getSQLState();
        return false;
    }catch(const std::exception& e2){
        LOGE << "std::exception: " << e2.what();
        return false;
    }catch(...){
        LOGE << "未知异常";
        return false;
    }
}

bool MysqlDao::GetUser(const int uid,UserInfo& userInfo){
    auto con = _mysqlPool->GetSqlConnection();
    if(con == nullptr){
        LOGE << "获取MysqlPool里的sql连接失败";
        return false;
    }

    SqlConnGuard guard(_mysqlPool.get(), std::move(con));

    try{
        // std::unique_ptr<sql::Statement> stmt(guard->_con->createStatement());
        // std::string sql = "SELECT * FROM user WHERE uid = " + std::to_string(uid);
        // std::unique_ptr<sql::ResultSet> ret(stmt->executeQuery(sql));
        /*  维度         Statement                                PreparedStatement
            SQL 文本     自己拼好整条 SQL 发给服务器              先发带 ? 的模板，再单独发参数值
            参数传递     拼字符串                                 pstmt->setInt(1, uid) / setString(2, name)
            转义         自己负责引号、反斜杠、\0 等              驱动负责，按类型编码
            SQL 注入     有风险，只要拼字符串就可能出问题         天然免疫
            特殊字符     例如 O'Brien 可能导致 SQL 语法错误       可以正常处理
            性能         每次都会解析 SQL                         语句可被服务端缓存、复用执行计划
            代码量       少一行                                   多一行 setInt / setString 等设置参数的代码*/
        std::unique_ptr<sql::PreparedStatement> pstmt(guard->_con->prepareStatement("SELECT * FROM user WHERE uid = ?"));
        pstmt->setInt(1, uid);
        std::unique_ptr<sql::ResultSet> ret(pstmt->executeQuery());
        bool hasNext = ret->next();
        if(hasNext){
            userInfo.pwd = ret->getString("pwd");
            userInfo.email = ret->getString("email");
            userInfo.name = ret->getString("name");
            userInfo.uid = ret->getInt("uid");
            userInfo.nick = ret->getString("nick");
			userInfo.desc = ret->getString("desc");
			userInfo.sex = ret->getInt("sex");
			userInfo.icon = ret->getString("icon");
            return true;
        }
        return false;
    }catch(const sql::SQLException& e){
        if (IsConnectionLost(e.getErrorCode())) guard.markBroken();
        LOGE << "SQLException: " << e.what()
             << ", code=" << e.getErrorCode()
             << ", SQLState=" << e.getSQLState();
        return false;
    }catch(const std::exception& e2){
        LOGE << "std::exception: " << e2.what();
        return false;
    }catch(...){
        LOGE << "未知异常";
        return false;
    }
}

bool MysqlDao::Ping(std::chrono::milliseconds timeout){
    auto con = _mysqlPool->GetSqlConnection(timeout);
    if(con == nullptr){
        LOGE << "获取MysqlPool里的sql连接失败";
        return false;
    }

    SqlConnGuard guard(_mysqlPool.get(), std::move(con));

    try{
        std::unique_ptr<sql::Statement> stmt(guard->_con->createStatement());
        std::unique_ptr<sql::ResultSet> rs(stmt->executeQuery("SELECT 1"));
        return rs->next() && rs->getInt(1) == 1;
    }catch(const sql::SQLException& e){
        if (IsConnectionLost(e.getErrorCode())) guard.markBroken();
        LOGE << "SQLException: " << e.what()
             << ", code=" << e.getErrorCode()
             << ", SQLState=" << e.getSQLState();
        return false;
    }catch(const std::exception& e2){
        LOGE << "std::exception: " << e2.what();
        return false;
    }catch(...){
        LOGE << "未知异常";
        return false;
    }
}

bool MysqlDao::AddFriendApply(const int& from_uid,const int& to_uid,const std::string& desc,const std::string& bakname){
    //未实现的重载：不能没有 return，否则是 UB
    return false;
}

bool MysqlDao::AddFriendApply(const int& from_uid,const int& to_uid){
    auto con = _mysqlPool->GetSqlConnection();     
    if(con == nullptr){
        LOGE << "获取MysqlPool里的sql连接失败";
        return false;
    } 

    SqlConnGuard guard(_mysqlPool.get(), std::move(con));

    try{
        std::unique_ptr<sql::PreparedStatement> pstmt(guard->_con->prepareStatement("INSERT INTO friend_apply (from_uid,to_uid) values (?,?) "
        "ON DUPLICATE KEY UPDATE from_uid = from_uid , to_uid = to_uid "));
        //ON DUPLICATE KEY UPDATE ...	MySQL 的 upsert 写法：插入时若撞上唯一键冲突，就改走 UPDATE 分支，而不是抛 1062
        //from_uid = from_uid, to_uid = to_uid	把字段设成自己 → 空操作（no-op），效果就是"已存在就什么都不做"
        pstmt->setInt(1,from_uid);
        pstmt->setInt(2,to_uid);
        //执行更新
        int rowAffected = pstmt->executeUpdate();
        if(rowAffected == 0)
            LOGE << "影响 0 行，条件没匹配到: from_uid=" << from_uid << " to_uid=" << to_uid;
        return true;//！！！！！！！ ON DUPLICATE KEY UPDATE from_uid = from_uid , to_uid = to_uid ——————> 所以return true
    }catch(const sql::SQLException& e){
        if (IsConnectionLost(e.getErrorCode())) guard.markBroken();
        LOGE << "SQLException: " << e.what()
             << ", code=" << e.getErrorCode()
             << ", SQLState=" << e.getSQLState();
        return false;
    }catch(const std::exception& e2){
        LOGE << "std::exception: " << e2.what();
        return false;
    }catch(...){
        LOGE << "未知异常";
        return false;
    }
}

    //为什么 before 是 long long、limit 是 int? apply.id 在库里是 bigint（64 位），游标必须也是 64 位，否则大 id 会溢出截断；
bool MysqlDao::GetApplyList(int uid, std::vector<std::shared_ptr<ApplyInfo>>& list, long long before_id, int limit){
    auto con = _mysqlPool->GetSqlConnection();
    if (con == nullptr) {                        
        LOGE << "获取MysqlPool里的sql连接失败";
        return false;
    }

    SqlConnGuard guard(_mysqlPool.get(), std::move(con));
    // before_id <= 0 表示"首页"，不加游标条件；否则 id < before_id（往老的方向翻）
    try{    //SQL 的执行顺序是：先 WHERE 过滤 → 再 ORDER BY 排序 → 最后 LIMIT 取前 N 条。
        std::string sql;
        sql = "select apply.id, apply.from_uid, apply.status, user.name, user.nick, user.sex, user.icon, user.desc "
        "from friend_apply as apply join user on apply.from_uid = user.uid "
        "where apply.to_uid = ? ";
        // "and apply.status = 0 ";
        if(before_id > 0)
            sql += "and apply.id < ? ";
        sql += "order by apply.id DESC "
        "LIMIT ?";
        
        std::unique_ptr<sql::PreparedStatement> pstmt(guard->_con->prepareStatement(sql));    //DESC ————> 拿最新的申请
        pstmt->setInt(1, uid);
        if(before_id > 0){
            pstmt->setInt64(2, before_id);
            pstmt->setInt(3, limit);
        }else{
            pstmt->setInt(2, limit);
        }
        /* excuteQuery()                            excuteUpdate()              execute()
返回类型：ResultSet*                                int(受影响行数)                 bool
能跑什么  只能跑会产生结果集的语句：SELECT / SHOW / DESCRIBE / EXPLAIN	
                    只能跑 DML/DDL：INSERT / UPDATE / DELETE / CREATE / DROP / ALTER	
                                                                                任意语句（通用）
返回值含义	结果集（要自己 delete）	        影响了几行（DDL 固定返回 0）	    execute() 的 true/false 含义：它不是"成功/失败"，
                                                        而是**"第一条结果是不是结果集"**。所以 execute() 返回 false 不代表出错，
                                                        而是"这次没有数据表，只有一个影响行数"，你得再 getUpdateCount() 去问。
                                                        存储过程 CALL 就属于这种"可能返回结果集、也可能只返回计数"的语句，所以只能用 execute()。
结果怎么拿	直接就是结果集	                返回值本身就是结果	                还要再调 getResultSet() / getUpdateCount()
典型场景	查列表	                        增删改（要行数）	                动态 SQL、存储过程、多结果集*/
        

        /*请问为什么一次只能加载这几条？为什么不一次全部拿到该向该用户的添加申请?
        1.  #define HEAD_ID_LEN    2      // msg_id
            #define HEAD_DATA_LEN  2      // msg_len
            #define HEAD_TOTAL_LEN 4    有效载荷最大大小为 2^16 = 64KB 

        2.  接收端有一条强制离线检查（CSession.cc）：
            if (msg_len > MAX_LENGTH) {          // MAX_LENGTH = 1024*2 = 2048
                LOGI << "invalid msg_len is " << msg_len << std::endl;
                Close();                          // ← 包体超 2048，直接断开连接
                _server->ClearSession(_session_id);
                return;
            }
            */
        std::unique_ptr<sql::ResultSet> result(pstmt->executeQuery());
        while(result->next()){
            int from_uid = result->getInt("from_uid");
            int status = result->getInt("status");
            std::string name = result->getString("name");
            std::string nick = result->getString("nick");
            int sex = result->getInt("sex");
            std::string icon = result->getString("icon");
            std::string desc = result->getString("desc");
            long long id = result->getInt64("id");
            list.push_back(std::make_shared<ApplyInfo>(from_uid, name, desc, icon, nick, sex, status, id)); 
            // LOGI << "从数据库中获取到Apply(from_uid: " << from_uid << " name: " << name << " desc: " << desc <<  " icon: " 
            // << icon << " nick: " << nick << " sex: " << sex << " status: " << status <<" id: " << id <<")";
        }
        return true;
    }catch(const sql::SQLException& e){
        if (IsConnectionLost(e.getErrorCode())) guard.markBroken();
        LOGE << "SQLException: " << e.what()
             << ", code=" << e.getErrorCode()
             << ", SQLState=" << e.getSQLState();
        return false;
    }catch(const std::exception& e2){
        LOGE << "std::exception: " << e2.what();
        return false;
    }catch(...){
        LOGE << "未知异常";
        return false;
    }
}

//？？Mysql的friend列表明显有问题————条目不全，现在暂时不解决，以后进行解决 ——————> 不对，可以补全，我以后要用全的信息时直接join不就行了吗
bool MysqlDao::GetFriendList(int uid, std::vector<std::shared_ptr<FriendInfo>>& list, long long before_id, int limit){
    auto con = _mysqlPool->GetSqlConnection();
    if (con == nullptr) {                        
        LOGE << "获取MysqlPool里的sql连接失败";
        return false;
    }

    SqlConnGuard guard(_mysqlPool.get(), std::move(con));
   // before_id <= 0 表示"首页"，不加游标条件；否则 id < before_id（往老的方向翻）
    try{    //SQL 的执行顺序是：先 WHERE 过滤 → 再 ORDER BY 排序 → 最后 LIMIT 取前 N 条。
        std::string sql;
        sql = "select friend.id, user.uid, user.name, user.nick, user.sex, user.icon, user.desc "
        "from friend join user on friend.friend_id = user.uid "
        "where friend.self_id = ? ";
        if(before_id > 0)
            sql += "and friend.id < ? ";
        sql += "order by friend.id DESC "
        "LIMIT ?";
        
        std::unique_ptr<sql::PreparedStatement> pstmt(guard->_con->prepareStatement(sql));    //DESC ————> 拿最新的申请
        pstmt->setInt(1, uid);
        if(before_id > 0){
            pstmt->setInt64(2, before_id);
            pstmt->setInt(3, limit);
        }else{
            pstmt->setInt(2, limit);
        }
        std::unique_ptr<sql::ResultSet> result(pstmt->executeQuery());
        while(result->next()){
            int friend_uid = result->getInt("uid");
            std::string name = result->getString("name");
            std::string nick = result->getString("nick");
            int sex = result->getInt("sex");
            std::string icon = result->getString("icon");
            std::string desc = result->getString("desc");
            long long id = result->getInt64("id");
            list.push_back(std::make_shared<FriendInfo>(friend_uid, name, desc, icon, nick, sex, id));   
        }
        return true;
    }catch(const sql::SQLException& e){
        if (IsConnectionLost(e.getErrorCode())) guard.markBroken();
        LOGE << "SQLException: " << e.what()
             << ", code=" << e.getErrorCode()
             << ", SQLState=" << e.getSQLState();
        return false;
    }catch(const std::exception& e2){
        LOGE << "std::exception: " << e2.what();
        return false;
    }catch(...){
        LOGE << "未知异常";
        return false;
    }
}

bool MysqlDao::GetRelation(int self_uid, int other_uid, int& relation){
    relation = -1; // -1：表示失败
    if(self_uid <= 0 || other_uid <= 0 || self_uid == other_uid){
        LOGE << "GetRelation 参数非法: self_uid=" << self_uid << " other_uid=" << other_uid;
        return false;
    }
    auto con = _mysqlPool->GetSqlConnection();
    if (con == nullptr) {                        
        LOGE << "获取MysqlPool里的sql连接失败";
        return false;
    }

    SqlConnGuard guard(_mysqlPool.get(), std::move(con));
   
    try{     
        std::unique_ptr<sql::PreparedStatement> pstmt(guard->_con->prepareStatement("SELECT "
            " EXISTS(SELECT 1 FROM friend WHERE self_id=? AND friend_id=?) AS is_friend,"
            " IFNULL((SELECT status FROM friend_apply WHERE from_uid=? AND to_uid=?), -1) AS my_apply,"
            " IFNULL((SELECT status FROM friend_apply WHERE from_uid=? AND to_uid=?), -1) AS other_apply"));    
        pstmt->setInt(1, self_uid);  pstmt->setInt(2, other_uid);
        pstmt->setInt(3, self_uid);  pstmt->setInt(4, other_uid);
        pstmt->setInt(5, other_uid);  pstmt->setInt(6, self_uid);

        std::unique_ptr<sql::ResultSet> result(pstmt->executeQuery());
        if(!result->next()){
            LOGE << "GetRelation 未返回任何结果行(SQL 可能被改动), self_uid=" << self_uid << " other_uid=" << other_uid;
            return false;
        }

        bool is_friend = (result->getInt("is_friend") != 0);
        // "my_apply" 和 "other_apply" 都是 返回的status    
        //status ——> -1: 没有此申请 0: 此申请正在等待审批 1: 此申请已经获得审批
        int mine_apply = result->getInt("my_apply");  
        int other_apply = result->getInt("other_apply");  
        if(is_friend) 
            relation = 1;   //是好友
        else if(mine_apply == 0 && other_apply == 0)
            relation = 4;   //双发申请都已通过(一般一方申请通过就已经互相添加为好友了，这种情况下只有一方删除好友然后又重新加上才会有这种情况)
        else if(mine_apply == 0)
            relation = 2;   //我已申请，等待对方
        else if(other_apply == 0)
            relation = 3;   //对方已申请，等待我
        else if(mine_apply == 1 || other_apply == 1){
            // 有"已同意"记录，但 friend 表里没有这条关系 —— 数据不一致
            LOGE << "GetRelation 数据不一致: apply.status=1 但 friend 表缺失, self_uid="
                 << self_uid << " other_uid=" << other_uid
                 << " my_status=" << mine_apply << " his_status=" << other_apply;
            relation = 5;                               
        }else{
            relation = 0;   //双方没有任何关系
        }
        return true;   
    }catch(const sql::SQLException& e){
        if (IsConnectionLost(e.getErrorCode())) guard.markBroken();
        LOGE << "SQLException: " << e.what()
             << ", code=" << e.getErrorCode()
             << ", SQLState=" << e.getSQLState();
        return false;
    }catch(const std::exception& e2){
        LOGE << "std::exception: " << e2.what();
        return false;
    }catch(...){
        LOGE << "未知异常";
        return false;
    }
}

bool MysqlDao::AuthFriendApply(const int& from_uid, const int& to_uid){
    auto con = _mysqlPool->GetSqlConnection();
    if(con == nullptr){
        LOGE << "获取MysqlPool里的sql连接失败";
        return false;
    }

    SqlConnGuard guard(_mysqlPool.get(), std::move(con));

    try{
        std::unique_ptr<sql::PreparedStatement> pstmt(guard->_con->prepareStatement("UPDATE friend_apply SET status = 1 "
        " WHERE from_uid = ? AND to_uid = ? "));
        pstmt->setInt(1,from_uid);
        pstmt->setInt(2,to_uid);
        int ret = pstmt->executeUpdate();
        if(ret == 0)
            LOGE << "UPDATE friend_apply SET status = 1  error !";
        return true;
    }catch(const sql::SQLException& e){
        if (IsConnectionLost(e.getErrorCode())) guard.markBroken();
        LOGE << "SQLException: " << e.what()
             << ", code=" << e.getErrorCode()
             << ", SQLState=" << e.getSQLState();
        return false;
    }catch(const std::exception& e2){
        LOGE << "std::exception: " << e2.what();
        return false;
    }catch(...){
        LOGE << "未知异常";
        return false;
    }
}

    /*信息	                     回答什么	                             用途
    what()	                发生了什么（人话）	                    给人看，不要拿来解析
    getErrorCode()	MySQL 的哪一条具体错误（1062 / 1146 / 2003…）	精确定位、写针对性的分支
    getSQLState()	属于哪一类错误（标准分类）	                    写通用策略、分类检索、跨数据库可移植*/

    //？？？？？？？？？？这里使用了参数不同指令相同的指令两次，但一个有bakname，一个没有 ——————>  这里还有优化空间
bool MysqlDao::AddFriend(const int& from_uid, const int& to_uid, const std::string& back_name){
    auto con = _mysqlPool->GetSqlConnection();
    if(con == nullptr){
        LOGE << "获取MysqlPool里的sql连接失败";
        return false;
    }
    
    SqlConnGuard guard(_mysqlPool.get(), std::move(con));     // ① 接管连接

    // ② 兜底：无论从哪条路径退出，都恢复 autocommit
    //    注意：它必须在 guard 之后声明 → 析构时先执行（先恢复状态，再归还/丢弃）
    Defer restoreAutoCommit([&guard](){
        try{
            guard->_con->setAutoCommit(true);
        }catch(...){
            guard.markBroken();          // 连 setAutoCommit 都失败 → 连接已死
        }
    });
    try{
        guard->_con->setAutoCommit(false);
        std::unique_ptr<sql::PreparedStatement> pstmt1(guard->_con->prepareStatement("INSERT IGNORE INTO friend(self_id,friend_id,back) "
        "VALUES (?,?,?)")); //向 friend 表中插入一条好友关系记录，但如果这条记录已经存在（比如违反了唯一索引），就直接忽略，不报错也不插入。
        // 同意 申请的 自己 ————> 这个是自己的好友数据
        pstmt1->setInt(1,from_uid);
        pstmt1->setInt(2,to_uid);
        pstmt1->setString(3,back_name);
        int rowAffected = pstmt1->executeUpdate();
        if(rowAffected == 0)            
            LOGE << "影响 0 行，条件没匹配到: from_uid=" << from_uid << " to_uid=" << to_uid;
        //插入对方的好友数据
        std::unique_ptr<sql::PreparedStatement> pstmt2(guard->_con->prepareStatement("INSERT IGNORE INTO friend(self_id,friend_id,back) "
        "VALUES (?,?,?)"));
        pstmt2->setInt(1,to_uid);
        pstmt2->setInt(2,from_uid);
        pstmt2->setString(3,"");  //？？？？？？？？？这里肯定不能这个备注设为空  
        int rowAffected_2 = pstmt2->executeUpdate();
        // 这里是错的  executeUpdate() 的语义是：成功返回受影响行数（≥0），失败直接抛异常。它不可能返回负数 此处根本不会有执行的机会
        // if(rowAffected_2 < 0){
        //     guard->_con->rollback();
        //     return false;
        // }
        if(rowAffected_2 == 0)
            LOGE << "影响 0 行，条件没匹配到: from_uid=" << to_uid << " to_uid=" << from_uid;
        guard->_con->commit();
        return true;
    }catch(const sql::SQLException& e){
        if (IsConnectionLost(e.getErrorCode())) guard.markBroken();
        try{
            guard->_con->rollback();
        }catch(const sql::SQLException& e2){
            LOGE << "rollback 失败（连接可能已断）";
            LOGE << "SQLException: " << e2.what() << ", code=" << e2.getErrorCode() << ", SQLState=" << e2.getSQLState();
        }catch(...){
            LOGE << "rollback()时出现未知错误";
        }
        LOGE << "由于某些SQL原因失败: " << e.what() << ", code=" << e.getErrorCode() << ", SQLState=" << e.getSQLState();
        return false;
    }catch(const std::exception& e){
        try{
            guard->_con->rollback();
        }catch(const sql::SQLException& e2){
            LOGE << "rollback 失败（连接可能已断）";
            LOGE << "SQLException: " << e2.what() << ", code=" << e2.getErrorCode() << ", SQLState=" << e2.getSQLState();
        }catch(...){
            LOGE << "rollback()时出现未知错误";
        }
        LOGE << "std:exception " << e.what();
        return false;
    }catch(...){
        try{
            guard->_con->rollback();
        }catch(const sql::SQLException& e2){
            LOGE << "恢复 autocommit 时 rollback 失败（连接可能已断）";
            LOGE << "SQLException: " << e2.what() << ", code=" << e2.getErrorCode() << ", SQLState=" << e2.getSQLState();
        }catch(...){
            LOGE << "rollback()时出现未知错误";
        }
        LOGE << "未知异常";
        return false;
    }
}