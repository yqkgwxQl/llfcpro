#include <jdbc/mysql_driver.h> //mysql 驱动程序
#include <jdbc/mysql_connection.h> //用于管理mysql数据库的连接
#include <jdbc/cppconn/statement.h> //用于sql语句的执行
#include <jdbc/cppconn/resultset.h> //用于获取查询的结果
#include <jdbc/cppconn/exception.h> //异常
#include <jdbc/cppconn/prepared_statement.h>
#include <iostream>
#include <string>
void addBook(sql::Connection* con);
void viewBooks(sql::Connection* con);
void updateBook(sql::Connection* con);
void deleteBook(sql::Connection* con);
//----------------------------------lesson1 简要体会c++mysql-------------------------------
// int main(){
//     try{
//         //1.创建mysql驱动对象
//         sql::mysql::MySQL_Driver* driver;
//         sql::Connection* con;
//         sql::Statement* stmt;
//         sql::ResultSet* res;
//             //初始化mysql驱动
//         driver = sql::mysql::get_mysql_driver_instance();
//         //2.建立与mysql的连接
//         con = driver->connect("tcp://127.0.0.1:3308","root","233520");
//         //3.选择sql_test数据库
//         con->setSchema("sql_test");
//         //4.创建sql语句
//         stmt = con->createStatement();
//         //5.执行sql语句
//         res = stmt->executeQuery("SELECT * FROM student");
//         //6.遍历并输出结果
//         while(res->next()){
//             std::cout << "-------------------------" << std::endl;
//             std::cout << "student_id :" << res->getInt("student_id") << std::endl;
//             std::cout << "student_name :" << res->getString("name") << std::endl;
//             std::cout << "student_major :" << res->getString("major") << std::endl;
//             std::cout << "student_score :" << res->getInt("score") << std::endl;
//             std::cout << "-------------------------" << std::endl;
//         }
//         //7.释放资源
//         delete res;
//         delete stmt;
//         delete con;
//     }catch (sql::SQLException& e){
//         std::cerr << "Mysql err :" << e.what() << std::endl;
//     }
// }

// int main(){
//     try{
//         //1.创建mysql驱动对象
//         sql::mysql::MySQL_Driver* driver;
//         sql::Connection* con;
//         sql::Statement* stmt;
//         sql::ResultSet* res;
//             //初始化mysql驱动
//         driver = sql::mysql::get_mysql_driver_instance();
//         //2.建立与mysql的连接
//         con = driver->connect("tcp://127.0.0.1:3308","root","233520");
//         //3.选择sql_test数据库
//         con->setSchema("sql_test");
//         //4.创建sql语句
        // stmt = con->createStatement();
        // //5.执行sql语句
        // res = stmt->executeQuery("SELECT * FROM books");
//         //6.遍历并输出结果
//         while(res->next()){
//             std::cout << "-------------------------" << std::endl;
//             std::cout << "book_id :" << res->getInt("book_id") << std::endl;
//             std::cout << "title :" << res->getString("title") << std::endl;
//             std::cout << "author :" << res->getString("author") << std::endl;
//             std::cout << "published_date :" << res->getString("published_date") << std::endl;
//             std::cout << "-------------------------" << std::endl;
//         }
//         //7.释放资源
//         delete res;
//         delete stmt;
//         delete con;
//     }catch (sql::SQLException& e){
//         std::cerr << "Mysql err :" << e.what() << std::endl;
//     }
// }

//----------------------------------lesson2 图书管理系统-------------------------------
int main(){
    try{
        sql::mysql::MySQL_Driver* driver = sql::mysql::get_driver_instance();
        sql::Connection* con = driver->connect("tcp://127.0.0.1:3308","root","233520");
        con->setSchema("sql_test");

        int choice;
        do{
            //显示菜单
            std::cout << "\n==== 图书管理系统 ====\n";
            std::cout << "1. 添加图书\n";
            std::cout << "2. 查看所有书籍\n";
            std::cout << "3. 更新书籍\n";
            std::cout << "4. 删除书籍\n";
            std::cout << "5. 退出系统\n";

            std::cin >> choice;

            if (std::cin.fail()) {  // 检查上一步的输入操作是否失败。
                std::cin.clear(); // 清除错误标志
                std::cin.ignore(10000, '\n'); // 最多跳过 10000 个字符，直到遇到换行符 '\n' 为止
                std::cout << "输入无效，请输入数字！\n";
                continue; 
            }
            switch(choice){
                case 1:
                    addBook(con);
                    break;
                case 2:
                    viewBooks(con);
                    break;
                case 3:
                    updateBook(con);
                    break;
                case 4:
                    deleteBook(con);
                    break;
                case 5:
                    std::cout << "退出系统\n";
                    break;
                default:
                    break;
            }
        }while(choice != 5);
    }catch(sql::SQLException& e){
        std::cerr << "Mysql err :" << e.what() << std::endl;
    }
}

void addBook(sql::Connection* con){
    try{
        /*为什么只用写一个std::cin.ignore();？
        缓冲区状态         操作                   结果
        ──────────────────────────────────────────────────
                            cin >> choice
                        用户输入 1 并回车
        1\n                 cin >> 读走 1
        \n                 ← 残留 \n

                            cin.ignore()          吃掉 \n
        （空）              ← 干净了

                            getline(cin, title)
                        用户输入 "test1" 并回车
        test1\n             getline 读走 "test1"
        \n                 ← getline 自动丢弃了 \n
        （空）              ← 干净了

                            getline(cin, author)  ← 不需要 ignore！
                        用户输入 "chen" 并回车
        chen\n              getline 读走 "chen"
        （空）              ← 干净了*/
        std::string title,author,date;
        std::cout << "输入书名：";
        std::cin.ignore(); // 这个函数的作用就是跳过并丢弃缓冲区里不需要的字符（比如换行符 \n），防止它们影响后续的输入操作
        std::getline(std::cin,title);
        
        std::cout << "输入作者：";
        // std::cin.ignore();
        std::getline(std::cin,author);

        std::cout << "输入出版日期(YYYY-MM-DD)：";
        // std::cin.ignore();
        std::getline(std::cin,date);
        // std::string query = "SELECT * FROM users WHERE name = \"" + userName + "\" AND pwd = '" + password + "'";
        /* 天然防 SQL 注入
        ------------什么是 SQL 注入?
        假设你在登录界面输入用户名和密码，后台用普通语句拼接 SQL：
        // 普通语句：直接把用户输入拼进 SQL
        std::string query = "SELECT * FROM users WHERE name = '" + userName + "' AND pwd = '" + password + "'";
        stmt->executeQuery(query);
        如果正常输入 张三 和 123456，拼出来的是：
        SELECT * FROM users WHERE name = '张三' AND pwd = '123456'
        但是攻击者可以这样输入
            用户名输入：admin' -- 密码随便填：xxx
            拼出来的 SQL 变成了： SELECT * FROM users WHERE name = 'admin' -- ' AND pwd = 'xxx'
            -- 在 SQL 中是注释符号，它后面的内容全部被忽略。所以实际执行的 SQL 是： SELECT * FROM users WHERE name = 'admin'
            不需要密码就登录了 admin 账号。
        更严重的攻击
            用户名输入：  '; DROP TABLE users; --
            拼出来后：  SELECT * FROM users WHERE name = ''; DROP TABLE users; --' AND pwd = 'xxx'
        ---------预编译做法（安全）：参数化查询
            1.程序先告诉 MySQL：“我要执行一个查询，结构是这样的，但我还没想好具体的值，先用问号占位。”
                SELECT * FROM users WHERE name = ? AND pwd = ?
                MySQL 收到后，会立即对这句话进行 语法分析 和 编译。它确定了这句话是一个“查询操作”，有两个“条件”。此时，SQL 的逻辑结构已经锁死了。
            2.发送数据（Bind & Execute）
                程序随后把具体的变量值发送给 MySQL。
                第一个 ? 对应 userName
                第二个 ? 对应 password
            关键点来了：
            当黑客再次输入 ' OR 1=1 -- 时，MySQL 不会 重新去解析这句话是不是 SQL 命令。因为在第一步中，MySQL 已经认定这里只能放“数据”。
            所以，MySQL 会把 ' OR 1=1 -- 当作一个完整的、普通的 纯文本字符串 存入那个位置
            最终执行的逻辑相当于  -- 寻找名字完全等于 "admin' OR 1=1 --" 的用户
            SELECT * FROM users WHERE name = "admin' OR 1=1 --" AND pwd = "xxx";
        ----------很多初学者都会认为预编译（PreparedStatement）仅仅是“把单引号换成了双引号”，或者“自动在两边加上了引号”。？？
            1.如果仅仅是“替换引号”，黑客依然能赢
                std::string query = "SELECT * FROM users WHERE name = \"" + userName + "\" AND pwd = '" + password + "'";
                黑客输入：" OR "1"="1
                SELECT * FROM users WHERE name = "" OR "1"="1" AND pwd = '...'
            2. 真正的原理：参数绑定与转义处理
                当你使用 setString() 方法时，C++ 的 MySQL 驱动和数据库服务器在内部做了两件极其重要的事情：
                第一步：SQL 结构提前锁死（预编译）
                    当你执行 con->prepareStatement("INSERT ... VALUES(?,?,?)") 时，包含 ? 的 SQL 模板被直接发送给数据库。数据库会立刻进行语法分析、编译并生成一个执行计划。在这个阶段，数据库已经明确知道：这里有三个位置是用来放数据的，绝对不允许出现任何 SQL 指令。
                第二步：数据作为纯文本传递（参数绑定）
                    当你调用 pstmt->setString(1, title) 时，用户输入的字符串是独立于 SQL 语句发送给数据库的4。JDBC/C++ 驱动程序会根据数据类型对特殊字符进行严格的转义处理
                    这意味着，无论黑客输入多么恶意的内容（比如 ' OR 1=1 --），数据库引擎都绝对不会把它当作可执行的 SQL 代码去解析，而是将其视为一个字面值（Literal String）
            3.setString() 并不是在做简单的引号替换游戏，它是通过底层的通信协议，将“指令”和“数据”分两条路发送，从而从根本上剥夺了用户输入破坏 SQL 结构的能力。这才是它安全的真正原因。
        */
        sql::PreparedStatement* pstmt = con->prepareStatement("INSERT INTO books(title,author,published_date) VALUES(?,?,?)");
        pstmt->setString(1,title);
        pstmt->setString(2,author);
        pstmt->setString(3,date);

        pstmt->execute();
        std::cout << "书籍添加成功\n";
    }catch(const std::exception& e){
        std::cerr << "std err :" << e.what() << std::endl;
    }
}
void viewBooks(sql::Connection* con){
    //这两个函数都是在堆上创建的，所以要delete掉
    sql::Statement* stmt = con->createStatement();  
    sql::ResultSet* ret = stmt->executeQuery("SELECT * FROM books");
    std::cout << "\n=====书籍列表=====\n";
    while(ret->next()){
        std::cout << "ID :" << ret->getInt("book_id") << std::endl;
        std::cout << "name :" << ret->getString("title") << std::endl;
        std::cout << "author :" << ret->getString("author") << std::endl;
        std::cout << "published_date :" << ret->getString("published_date") << std::endl;
        std::cout << "----------------------------\n";
    }
    delete stmt;
    delete ret;
}
void updateBook(sql::Connection* con){
    viewBooks(con);
    int id;
    std::string title,author,date;
    std::cout << "请输入你想更新书籍对应的id"<< std::endl;
    std::cin >> id;
    std::cin.ignore(); 
    std::cout << "输入书名：";
    std::getline(std::cin,title);
    
    std::cout << "输入作者：";
    std::getline(std::cin,author);

    std::cout << "输入出版日期(YYYY-MM-DD)：";
    std::getline(std::cin,date);
    
    sql::PreparedStatement* pstmt = con->prepareStatement("UPDATE books SET title = ?, author = ?, published_date = ?");
    pstmt->setString(1,title);
    pstmt->setString(2,author);
    pstmt->setString(3,date);

    pstmt->execute();
    std::cout << "更新书籍成功\n";
    viewBooks(con);
    delete pstmt;
}
void deleteBook(sql::Connection* con){
    viewBooks(con);
    std::cout << "请输入你想删除书籍的id" << std::endl;
    int id = 0;
    std::cin >> id;
    std::cin.ignore();
    sql::PreparedStatement* pstmt = con->prepareStatement("DELETE FROM books WHERE book_id = ?");
    pstmt->setInt(1,id);
    pstmt->execute();
    std::cout << "删除书籍成功\n";
    viewBooks(con);
    delete pstmt;
}