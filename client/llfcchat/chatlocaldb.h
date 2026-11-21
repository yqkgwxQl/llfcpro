#ifndef CHATLOCALDB_H
#define CHATLOCALDB_H

#include "singleton.h"
#include <QSqlDatabase>

class ChatDataBase;

class ChatLocalDb: public Singleton<ChatLocalDb>
{
    friend class Singleton<ChatLocalDb>;
public:
    enum class SaveResult{
        OK = 0,             // 已写入本地库
        NOT_OPEN = 1,       // 库没打开（降级模式）
        WRITE_FAILED = 2,   // 库开着，但这次写失败
    };
    ~ChatLocalDb();

    bool Open(int self_uid);        // 打开chatcache_<self_uid>.db(why要传self_uid,因为可能切换用户导致self_uid不一样，要切到self_uid对应db上) 然后需要建表
    bool Close();
    SaveResult SaveMsg(int other_uid, const std::shared_ptr<ChatDataBase> msg);
    bool UpdateStatus(const QString& unique_id, MsgStatus status);
    bool LoadSession();
    bool Clear();
    bool IsOpen()const;
    int GetSelfUid();
private:
    ChatLocalDb();
    bool CreateTables();
    QString DbPath(int self_uid)const;

    QSqlDatabase _db;
    int _self_uid;  // 当前缓存里使用的是哪个self_uid 使用的是哪个self_uid对应的database
};

#endif // CHATLOCALDB_H
