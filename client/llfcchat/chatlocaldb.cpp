#include "chatlocaldb.h"
#include <QDir>
#include <QCoreApplication>
#include <QStandardPaths>
#include <QSqlQuery>
#include <QSqlError>
#include "userdata.h"
#include "usermgr.h"
#include <QFile>
#include <QFileInfo>

ChatLocalDb::~ChatLocalDb(){
    Close();
}

bool ChatLocalDb::Open(int self_uid){
    if(self_uid <= 0){
        qDebug() << "ChatLocalDb::Open: 非法uid: " << self_uid;
        return false;
    }
    if(IsOpen() && self_uid == _self_uid)       //这里应该是用户重复登录一个账户，复用现有连接
        return true;
    Close();        // 换账号：先彻底关掉旧的
    if(!QSqlDatabase::isDriverAvailable(QStringLiteral("QSQLITE"))){
        qDebug()  << "ChatLocalDb: 没有 QSQLITE 驱动！可用驱动 =" << QSqlDatabase::drivers();
        return false;
    }
    const QString path = DbPath(self_uid);
    if(!QDir().mkpath(QFileInfo(path).absolutePath())){ // 这里是创建目录，_db.open()会自动创建文件，but不能自动创建目录
        qDebug() << "ChatLocalDb: 创建目录失败" << QFileInfo(path).absoluteFilePath();
        return false;
    }
    /*一句话："QSQLITE" 是驱动名，告诉 Qt「用哪一套后端来执行 SQL」
        QSqlDatabase::addDatabase("QSQLITE", "llfc_chatcache");
        //                            ↑ type（驱动名）   ↑ connectionName（这次连接的名字）
        Qt 的 SQL 模块是分层的：上层（QSqlDatabase / QSqlQuery / QSqlRecord）是通用接口，下层是可插拔的驱动插件。

        你的代码
           │  QSqlQuery::exec("INSERT INTO msg ...")
           ▼
        QSqlDatabase / QSqlQuery        ← Qt 的通用外壳，自己不认识任何 SQL 方言
           │  按驱动名找到对应插件
           ▼
        qsqlite.dll                    ← 驱动插件（plugins/sqldrivers/）
           │  把 Qt 的调用翻译成 SQLite 的 C API
           ▼
        SQLite 3.x（静态链接在插件里）    ← 真正的执行引擎，读写 .db 文件
        所以 "QSQLITE" 决定了：底层是 SQLite 引擎、操作的是一个本地文件、说的是 SQLite 方言。*/
    _db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), QStringLiteral("llfc_chatcache"));
    _db.setDatabaseName(path);      //告诉QSQLITE， llfc_chatcache的真实路径
    if(!IsOpen()){
        qDebug() << "ChatLocalDb: 打开失败" << path << _db.lastError().text();
        Close();
        return false;
    }
    QSqlQuery pragma(_db);
    pragma.exec(QStringLiteral("PRAGMA journal_mode=WAL"));     // 读不阻塞写
    pragma.exec(QStringLiteral("PRAGMA synchronous=NORMAL"));   // 断电最多丢最后几条，换写入速度
    pragma.exec(QStringLiteral("PRAGMA busy_timeout=3000"));   // 遇到锁等 3 秒，而不是立刻报错

    _self_uid = self_uid;
    if(!CreateTables()){
        Close();
        return false;
    }

    qDebug() << "ChatLocalDb 打开成功" << path;
    return true;
}

/*QSqlDatabase 对象是一层句柄，它本身并不代表数据库连接
    Qt 把所有数据库连接放在一个进程级的全局注册表里，用「连接名」当 key：

    QSqlDatabase::addDatabase("QSQLITE", "llfc_chatcache");
    //                                    ↑ 名字就是这个注册表的 key
    内存里长这样：
        全局注册表（QSqlDatabase 类的静态成员，整个进程一份）
        ┌──────────────────┬───────────────────────────┐
        │ "llfc_chatcache" │ → QSqlDatabasePrivate ──→ 真正的 SQLite 连接（持有 .db 文件句柄）
        └──────────────────┴───────────────────────────┘
                                      ↑
                                _db 这个对象只是"引用它"的句柄
    QSqlDatabase 是隐式共享的：QSqlDatabase b = a; 不会复制连接，只是让两个句柄指向同一个 Private，内部有个引用计数。

    所以 _db.close() 和"把连接从注册表里删掉"是两件不同的事。*/
bool ChatLocalDb::Close(){
    if(IsOpen())   //关掉物理连接, 关闭打开的文件
        _db.close();

    _db = QSqlDatabase();               // 丢掉手里的句柄, 引用计数 -1

    if(QSqlDatabase::contains("llfc_chatcache"))        //从全局注册表里删除此条目
        QSqlDatabase::removeDatabase(QStringLiteral("llfc_chatcache"));

    _self_uid = 0;
    return true;
}

ChatLocalDb::SaveResult ChatLocalDb::SaveMsg(int other_uid, const std::shared_ptr<ChatDataBase> msg){
    if(other_uid <= 0 || !msg || msg->GetUniqueId().isEmpty())
        return SaveResult::WRITE_FAILED;
    if(!IsOpen())
        return SaveResult::NOT_OPEN;
    QSqlQuery q(_db);
    q.prepare(QStringLiteral(
        "INSERT OR IGNORE INTO msg(unique_id, other_uid, send_uid, msg_type, content, status, chat_time) "
        "VALUES(?,?,?,?,?,?,?)"));
    q.addBindValue(msg->GetUniqueId());
    q.addBindValue(other_uid);
    q.addBindValue(msg->GetSendUid());
    q.addBindValue(static_cast<int>(msg->GetMsgType()));
    q.addBindValue(msg->GetContent());
    q.addBindValue(static_cast<int>(msg->GetStatus()));
    q.addBindValue(msg->GetChatTime());

    if(!q.exec()){
        qDebug() << "ChatLocalDb::SaveMsg 失败" << q.lastError().text() << msg->GetUniqueId();
        return SaveResult::WRITE_FAILED;
    }
    return SaveResult::OK;
}

bool ChatLocalDb::UpdateStatus(const QString& unique_id, MsgStatus status){
    if(!IsOpen())
        return false;
    QSqlQuery q(_db);
    q.prepare(QStringLiteral(
        "UPDATE msg SET status = ?"
        " WHERE unique_id = ?"));
    q.addBindValue(static_cast<int>(status));
    q.addBindValue(unique_id);
    if(!q.exec()){
        qDebug() << "ChatLocalDb::UpdataStatus 失败" << q.lastError().text() << unique_id;
        return false;
    }
    return true;
}

bool ChatLocalDb::LoadSession(){
    if(!IsOpen())
        return false;
    QSqlQuery q(_db);
    q.prepare(QStringLiteral(
        "SELECT unique_id, other_uid, send_uid, msg_type, content, status, chat_time"
        " FROM msg ORDER BY seq ASC"));

    if(!q.exec()){
        qDebug() << "ChatLocalDb::LoadSessions 查询失败" << q.lastError().text();
        return false;
    }

    int loading = 0;
    while(q.next()){
        const QString unique_id = q.value(0).toString();
        const int     other_uid = q.value(1).toInt();
        const int     send_uid  = q.value(2).toInt();
        const ChatMsgType    msg_type  = static_cast<ChatMsgType>(q.value(3).toInt());   // 见下面的 clamp
        const QString content   = q.value(4).toString();
        MsgStatus     status    = static_cast<MsgStatus>(q.value(5).toInt());
        const QString chat_time = q.value(6).toString();

        // 上次进程退出时还没等到 1018 的消息，不能让它永远停在 SENDING
        if(status == MsgStatus::SENDING)
            status = MsgStatus::FAILED;

        auto session = UserMgr::GetInstance()->GetChatSessionByUid(other_uid);
        if(!session)
            continue;
        session->AppendMsg(unique_id, std::make_shared<ChatDataBase>(unique_id, send_uid, msg_type, content, status, chat_time), false);

        loading++;
    }
    qDebug() << "ChatLocalDb: 回读消息" << loading << "条";
    return true;
}

//清理本地数据
bool ChatLocalDb::Clear(){
    if(!IsOpen())
        return false;
    QSqlQuery q(_db);
    if(!q.exec(QStringLiteral("DELETE FROM msg"))){
        qDebug() << "ChatLocalDb::Clear 失败" << q.lastError().text();
        return false;
    }
    // sqlite_sequence： SQLite 自动创建、自动维护的一本"自增账本"
    q.exec(QStringLiteral("DELETE FROM sqlite_sequence WHERE name='msg'"));  // 让 seq 从 1 重新开始
    return true;
}

bool ChatLocalDb::IsOpen()const{
    return _db.isValid() && _db.isOpen();
}

int ChatLocalDb::GetSelfUid(){
    return _self_uid;
}


ChatLocalDb::ChatLocalDb()
    :_self_uid(0){
    _db = QSqlDatabase();
}

/*
    MySQL = 服务端的真源（多用户、跨设备、离线消息都靠它）
    SQLite = 客户端的缓存（单用户、断网可读、可删可重建）
*/
//这里使用的时SQLite，而非MySQL
bool ChatLocalDb::CreateTables(){
    QSqlQuery q(_db);
    const QStringList ddls = {
        // QStringLiteral 相当于编译期的QString
        QStringLiteral(
            "CREATE TABLE IF NOT EXISTS msg("
            " seq        INTEGER PRIMARY KEY AUTOINCREMENT," // 本地顺序号 = 排序游标
            " unique_id  TEXT    NOT NULL UNIQUE,"
            " other_uid   INTEGER NOT NULL,"              // 会话 key = 对话另一方（必须单独存，见下
            " send_uid   INTEGER NOT NULL,"              // 真实发送者（决定气泡在左还是右）
            " msg_type   INTEGER NOT NULL,"
            " content    TEXT    NOT NULL,"
            " status     INTEGER NOT NULL,"
            " chat_time  TEXT    NOT NULL DEFAULT ''"   // 这一列永远不能是 NULL；如果你忘了给，就自动填空串，而不是让整条语句失败
            " )"),
        QStringLiteral("CREATE INDEX IF NOT EXISTS idx_msg_session ON msg(other_uid, seq)")
        /*idx_msg_session（按 other_uid 升序，同 other_uid 内按 seq 升序）
            ┌──────────────────────┬───────────────┐
            │  键 (other_uid, seq)  │  指向哪一行    │
            ├──────────────────────┼───────────────┤
            │      (14, 2)         │   → 第 2 行   │
            │      (14, 3)         │   → 第 3 行   │
            │      (15, 1)         │   → 第 1 行   │
            │      (15, 4)         │   → 第 4 行   │
            └──────────────────────┴───────────────┘*/
    };
    for(const QString& ddl : ddls){
        if(!q.exec(ddl)){
            qDebug() << "ChatLocalDb: 建表失败" << q.lastError().text();
            return false;
        }
    }
    return true;
}

//算出「聊天缓存数据库文件应该放在哪个路径」
    /*
    1.QStringdir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    这是 Qt 提供的跨平台标准目录查询。你告诉它"我要一个当前用户可写的、属于本应用的数据目录"，它按平台规则算出来：
        平台          AppDataLocation 返回
        Windows     C:/Users/<用户名>/AppData/Roaming/<应用名>
        macOS       ~/Library/Application Support/<应用名>
        Linux       ~/.local/share/<应用名>
    在你机器上（$env:APPDATA = C:\Users\<用户名>\AppData\Roaming，应用名默认取 exe 名 llfcchat）：
        C:/Users/<用户名>/AppData/Roaming/llfcchat
    所以最终路径是：
        C:/Users/<用户名>/AppData/Roaming/llfcchat/chatcache_15.db

    2.为什么不用 exe 所在目录？
    你可能会想：QCoreApplication::applicationDirPath() + "/chatcache.db" 不是更简单？不行，三个理由：

    问题                                                  说明
    1_可能没有写权限                   程序装到 C:\Program Files\ 下时，普通用户对该目录只读。写文件会被 UAC 虚拟化重定向到别处，或者直接失败
    2_多用户会互相覆盖	同一台机器上      A、B 两个账号用你的程序，exe 目录只有一份 → 两人共用同一个 .db，聊天记录串了
    3_不是用户数据的规范位置           用户卸载/备份/清理时，知道去 AppData 找应用数据；散落在安装目录里既不规范也容易误删

    AppData 就是干这个的：每个用户一份、一定可写、系统不会随便清理。
    顺带在你 main.cpp 里那句长注释上讲过同样的事——config.ini 用 applicationDirPath() 是为了读（程序自带的配置），而 .db 是运行时产生的用户数据，必须走 AppData。读和写，位置策略不同。

    本项目采用exe所在目录 ——————> 1_ 仅为单一用户<用户名> 2_ 方便开发过程中进行调试
    */

namespace{
    //真的去写一个探针文件
    bool IsDirWritable(const QString& dir){
        if(!QDir().mkpath(dir))
            return false;

        //有可能同一台机器上切换用户，上面的目录已经存在了，但是权限是不可写的，therefore要进行下面的探针测试
        QFile probe(QDir(dir).filePath(QStringLiteral(".write_probe")));
        if(!probe.open(QIODevice::WriteOnly))
            return false;
        probe.close();
        probe.remove();
        return true;
    }
};

QString ChatLocalDb::DbPath(int self_uid) const{
    QString dir = QCoreApplication::applicationDirPath() + QStringLiteral("/data");
    if(!IsDirWritable(dir)){
        const QString standard_dir = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
        qDebug() << "ChatLocalDb: exe 目录不可写，改用" << standard_dir;
        dir = standard_dir;
    }
    return QDir(dir).filePath("chatcache_%1.db").arg(self_uid);
}


/*QT 文件操作相关API
    ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
    第 5 层  应用/标准目录     QCoreApplication / QStandardPaths     "程序在哪 / 用户数据该放哪"
    第 4 层  路径计算（不碰磁盘） QDir                                  "拼路径、判断绝对/相对"
    第 3 层  文件信息（只查）    QFileInfo                              "存在吗？大小？父目录是谁？"
    第 2 层  目录磁盘操作       QDir 的磁盘方法                          "建目录、删目录、列目录"
    第 1 层  文件内容读写       QFile / QSaveFile                       "真正读写字节"
    ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
    第 5 层：应用与标准目录
    API                                                 返回什么                        例子（你的机器）
    QCoreApplication::applicationDirPath()              exe 所在目录，无尾部分隔符      E:/zhongzhuan2/llfcchatpro/client/llfcchat/build
    QCoreApplication::applicationFilePath()             exe 完整路径                    .../build/llfcchat.exe
    QStandardPaths::writableLocation(枚举)              标准目录（由系统规则决定）      见下表

    QStandardPaths 常用的枚举：
        枚举                          Windows 返回值                                      该放什么
        AppDataLocation               C:/Users/<用户>/AppData/Roaming/<应用名>            用户配置（会随域漫游）
        AppLocalDataLocation          C:/Users/<用户>/AppData/Local/<应用名>              缓存、数据库、大文件
        TempLocation                  C:/Users/<用户>/AppData/Local/Temp                  临时文件
        DocumentsLocation             C:/Users/<用户>/Documents                           用户可见的文档
        ApplicationsLocation          C:/ProgramData/Microsoft/Windows/Start Menu/Programs 开始菜单（装快捷方式用的）

    <应用名> 来自 QCoreApplication::applicationName()，默认取 exe 名。
    ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
    第 2 层：QDir —— 路径计算（不碰磁盘）
    QDir dir("E:/a/b");

    dir.filePath("x.db");          // → "E:/a/b/x.db"        ← 拼接（不检查存在）
    dir.absoluteFilePath("x.db");  // → "E:/a/b/x.db"        ← 先转绝对再拼
    dir.exists();                  // → 目录是否存在
    dir.absolutePath();            // → "E:/a/b"             ← 自己的绝对路径
    QDir::currentPath();           // → 当前工作目录
    QDir::homePath();              // → C:/Users/22688
    QDir::cleanPath("a/./b/../c"); // → "a/c"                ← 去掉 . 和 ..
    QDir::isAbsolutePath("E:/x");  // → true
    QDir() 无参构造 = 当前工作目录。所以 QDir().mkpath(绝对路径) 就等于"创建这个绝对路径"，这是最常见的写法。
        QDir() 当参数是绝对路径时，mkpath 会忽略 QDir 自己的路径 ； 和 filePath() 的规则一致：参数是绝对路径时，对象自身的路径被覆盖。
    ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
    第 3 层：QFileInfo —— 只查不改
    QFileInfo fi("E:/a/b/chatcache_15.db");

    fi.exists();            // 存在吗
    fi.isFile() / isDir();  // 是文件还是目录
    fi.size();              // 字节数
    fi.isWritable();        // 可写吗（看权限位/ACL）
    fi.lastModified();      // 修改时间
    fi.fileName();          // "chatcache_15.db"          ← 文件名（带扩展名）
    fi.suffix();            // "db"                       ← 扩展名
    fi.completeBaseName();  // "chatcache_15"             ← 去掉扩展名
    fi.absolutePath();      // "E:/a/b"                   ← ★ 父目录（不含文件名）
    fi.absoluteFilePath();  // "E:/a/b/chatcache_15.db"   ← 完整绝对路径
    fi.canonicalFilePath(); // 解析软链接/8.3短名后的真实路径
    absolutePath() 就是你那段代码用的那个：给一个文件路径，返回它所在的目录。所以


    QDir().mkpath(QFileInfo("/x/y/z.db").absolutePath());   // 建 /x/y
    等价于"建这个文件将要放进的那个目录"。
    ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
    第 4 层：QDir 的磁盘操作（会真的动磁盘）
    方法                    作用                    注意
    mkpath("a/b/c")         递归创建所有父目录      ✅ 目录已存在时返回 true（什么也不做）
    mkdir("c")              只创建最后一级          ❌ 父目录不存在就失败
    rmdir("c")              删空目录                非空会失败
    removeRecursively()     递归删除                ⚠️ 危险
    entryList()             列目录内容              返回文件名列表
    ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
    第 1 层：QFile —— 读写内容
    QFile f("E:/a/b/x.txt");

    f.open(QIODevice::WriteOnly);            // 只写（会截断已有内容）
    f.open(QIODevice::WriteOnly | QIODevice::Append);   // 追加
    f.open(QIODevice::ReadWrite);            // 读写
    f.write("hello");
    f.close();

    // 静态快捷方法（不需要先 open）
    QFile::exists("x.txt");
    QFile::remove("x.txt");
    QFile::copy("a.txt", "b.txt");
    QFile::rename("a.txt", "b.txt");
    QIODevice::WriteOnly 这些是打开模式标志，QFile 继承自 QIODevice，所以 open() 的参数就是它们。

    写配置文件应该用 QSaveFile：它先写临时文件、commit() 时才原子替换，中途崩溃不会把原文件写
    ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
    ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
*/
