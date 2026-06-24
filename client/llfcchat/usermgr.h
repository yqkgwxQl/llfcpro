#ifndef USERMGR_H
#define USERMGR_H
#include <memory>
#include "singleton.h"
#include <QString>
#include "userdata.h"

class UserMgr:public Singleton<UserMgr>,public std::enable_shared_from_this<UserMgr>
{
    friend class Singleton<UserMgr>;
public:
    ~UserMgr();
    void SetName(QString name);
    void SetUid(int uid);
    void SetToken(QString token);
    QString GetName();
    std::vector<std::shared_ptr<ApplyInfo>> GetApplyList();
private:
    UserMgr();
    QString _name;
    QString _token;
    int _uid;
    std::vector<std::shared_ptr<ApplyInfo>> _apply_list;
};

#endif // USERMGR_H
