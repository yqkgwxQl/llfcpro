#include "MysqlDao_2.h"
#include "ConfigMgr.h"
#include <iostream>
#include <cstring>

MysqlDao_2::MysqlDao_2() {
    auto& cfg = ConfigMgr::GetInstance();
    const auto& host = cfg["Mysql"]["Host"];
    const auto& port = cfg["Mysql"]["Port"];
    const auto& pwd = cfg["Mysql"]["Passwd"];
    const auto& schema = cfg["Mysql"]["Schema"];
    const auto& user = cfg["Mysql"]["User"];

    _mysqlPool.reset(new MysqlPool_2(host, std::stoi(port), user, pwd, schema, 5));
}

MysqlDao_2::~MysqlDao_2() {
    if (_mysqlPool) {
        _mysqlPool->Close();
    }
}

int MysqlDao_2::RegUser(const std::string& name, const std::string& email, const std::string& pwd) {
    auto con = _mysqlPool->GetSqlConnection();
    if (con == nullptr) {
        return -1;
    }

    MYSQL* mysql = con->_con.get();

    try {
        // 转义参数防止 SQL 注入
        char escaped_name[256], escaped_email[256], escaped_pwd[256];
        mysql_real_escape_string(mysql, escaped_name, name.c_str(), (unsigned long)name.size());
        mysql_real_escape_string(mysql, escaped_email, email.c_str(), (unsigned long)email.size());
        mysql_real_escape_string(mysql, escaped_pwd, pwd.c_str(), (unsigned long)pwd.size());

        // 执行存储过程 CALL reg_user('name','email','pwd',@result)
        //这里调用了reg_user函数 ————>  mysql库里
        std::string query = "CALL reg_user('" + std::string(escaped_name) + "','"
                            + std::string(escaped_email) + "','"
                            + std::string(escaped_pwd) + "',@result)";

        if (mysql_query(mysql, query.c_str()) != 0) {
            std::cerr << "CALL reg_user failed: " << mysql_error(mysql) << std::endl;
            _mysqlPool->ReturnSqlConnection(std::move(con));
            return -1;
        }

        // 消耗掉存储过程可能返回的结果集（如果有的话）
        MYSQL_RES* result;
        while ((result = mysql_store_result(mysql)) != nullptr) {
            mysql_free_result(result);
            if (mysql_next_result(mysql) != 0) {
                break;
            }
        }

        // 查询 @result 输出参数
        if (mysql_query(mysql, "SELECT @result AS result") != 0) {
            std::cerr << "SELECT @result failed: " << mysql_error(mysql) << std::endl;
            _mysqlPool->ReturnSqlConnection(std::move(con));
            return -1;
        }

        MYSQL_RES* res = mysql_store_result(mysql);
        if (res == nullptr) {
            std::cerr << "mysql_store_result failed: " << mysql_error(mysql) << std::endl;
            _mysqlPool->ReturnSqlConnection(std::move(con));
            return -1;
        }

        MYSQL_ROW row = mysql_fetch_row(res);
        int ret = -1;
        if (row != nullptr && row[0] != nullptr) {
            ret = std::stoi(row[0]);
            std::cout << "Result: " << ret << std::endl;
        }

        mysql_free_result(res);
        _mysqlPool->ReturnSqlConnection(std::move(con));
        return ret;

    } catch (std::exception& e) {
        std::cerr << "RegUser exception: " << e.what() << std::endl;
        _mysqlPool->ReturnSqlConnection(std::move(con));
        return -1;
    }
}


bool MysqlDao_2::CheckEmail(const std::string& name, const std::string& email) {
    auto con = _mysqlPool->GetSqlConnection();
    if (con == nullptr) {
        return false;
    }

    MYSQL* mysql = con->_con.get();

    try {
        // 转义参数防止 SQL 注入
        char escaped_name[256];
        mysql_real_escape_string(mysql, escaped_name, name.c_str(), (unsigned long)name.size());

        std::string query = "SELECT email FROM user WHERE name = '" + std::string(escaped_name) + "'";

        if (mysql_query(mysql, query.c_str()) != 0) {
            std::cerr << "CheckEmail query failed: " << mysql_error(mysql) << std::endl;
            _mysqlPool->ReturnSqlConnection(std::move(con));
            return false;
        }

        MYSQL_RES* res = mysql_store_result(mysql);
        if (res == nullptr) {
            std::cerr << "CheckEmail mysql_store_result failed: " << mysql_error(mysql) << std::endl;
            _mysqlPool->ReturnSqlConnection(std::move(con));
            return false;
        }

        MYSQL_ROW row = mysql_fetch_row(res);
        if (row == nullptr || row[0] == nullptr) {
            // 没有查到该用户
            std::cout << "CheckEmail: user not found" << std::endl;
            mysql_free_result(res);
            _mysqlPool->ReturnSqlConnection(std::move(con));
            return false;
        }

        std::string db_email(row[0]);
        std::cout << "Check Email: " << db_email << std::endl;

        bool match = (email == db_email);
        mysql_free_result(res);
        _mysqlPool->ReturnSqlConnection(std::move(con));
        return match;

    } catch (std::exception& e) {
        std::cerr << "CheckEmail exception: " << e.what() << std::endl;
        _mysqlPool->ReturnSqlConnection(std::move(con));
        return false;
    }
}

bool MysqlDao_2::UpdatePwd(const std::string& name, const std::string& newpwd) {
    auto con = _mysqlPool->GetSqlConnection();
    if (con == nullptr) {
        return false;
    }

    MYSQL* mysql = con->_con.get();

    try {
        // 转义参数防止 SQL 注入
        char escaped_name[256], escaped_pwd[256];
        mysql_real_escape_string(mysql, escaped_name, name.c_str(), (unsigned long)name.size());
        mysql_real_escape_string(mysql, escaped_pwd, newpwd.c_str(), (unsigned long)newpwd.size());

        std::string query = "UPDATE user SET pwd = '" + std::string(escaped_pwd)
                          + "' WHERE name = '" + std::string(escaped_name) + "'";

        if (mysql_query(mysql, query.c_str()) != 0) {
            std::cerr << "UpdatePwd query failed: " << mysql_error(mysql) << std::endl;
            _mysqlPool->ReturnSqlConnection(std::move(con));
            return false;
        }

        int updateCount = (int)mysql_affected_rows(mysql);
        std::cout << "Updated rows: " << updateCount << std::endl;

        _mysqlPool->ReturnSqlConnection(std::move(con));
        return updateCount > 0;

    } catch (std::exception& e) {
        std::cerr << "UpdatePwd exception: " << e.what() << std::endl;
        _mysqlPool->ReturnSqlConnection(std::move(con));
        return false;
    }
}