#include "chatlocaldb.h"
#include <QDir>
#include <QCoreApplication>
#include <QStandardPaths>
#include <QSqlQuery>

ChatLocalDb::~ChatLocalDb(){

}

bool ChatLocalDb::Open(int self_uid){
    if(self_uid < 0){
        qDebug() << "ChatLocalDb::Open: 非法uid: " << self_uid;
        return false;
    }

    if(_db.isOpen() && self_uid == _self_uid)       //这里应该是用户重复登录一个账户，复用现有连接
        return true;

    Close();        // 换账号：先彻底关掉旧的

}

bool ChatLocalDb::Close(){
    if(_db.isValid() && _db.isOpen())   //关掉物理连接
        _db.close();

    _db = QSqlDatabase();               // 丢掉手里的句柄

    if(QSqlDatabase::contains("llfc_cahtcache"))        //从全局注册表里删除此条目
        QSqlDatabase::removeDatabase(QStringLiteral("llfc_chatcache"));

    _self_uid = 0;
}

ChatLocalDb::ChatLocalDb(){

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
            "CREATE TABLE IF NOT EXIST msg("
            " seq        INTEGER PRIMARY AUTOINCREMENT" // 本地顺序号 = 排序游标
            " unique_id  TEXT    NOT NULL UNIQUE"
            " other_id   INTEGER NOT NULL"              // 会话 key = 对话另一方（必须单独存，见下
            " send_uid   INTEGER NOT NULL"              // 真实发送者（决定气泡在左还是右）
            " msg_type   INTEGER NOT NULL"
            " content    TEXT    NOT NULL"
            " status     INTEGER NOT NULL"
            " chat_time  TEXT    NOT NULL DEFAULT ''"   // 这一列永远不能是 NULL；如果你忘了给，就自动填空串，而不是让整条语句失败
            " )"),
        QStringLiteral("CREATE INDEX IF NOT EXIST idx_msg_session ON msg(other_uid, seq")
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
    QString dir = QCoreApplication::applicationDirPath() + QStringLiteral("data");
    if(!IsDirWritable(dir)){
        const QString standard_dir = QStandardPaths::writableLocation(QStandardPaths::ApplicationsLocation);
        qDebug() << "ChatLocalDb: exe 目录不可写，改用" << standard_dir;
        dir = standard_dir;
    }
    return QDir(dir).filePath("chatcache_%1.db").arg(self_uid);
}

