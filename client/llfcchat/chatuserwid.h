#ifndef CHATUSERWID_H
#define CHATUSERWID_H

#include <QWidget>
#include "listitembase.h"
#include <QSize>

namespace Ui {
class ChatUserWid;
}

class AuthInfo;
class AuthRsp;
class UserInfo;
class ChatSessionData;

class ChatUserWid : public ListItemBase
{
    Q_OBJECT

public:
    explicit ChatUserWid(QWidget *parent = nullptr);
    ~ChatUserWid();
    QSize sizeHint()const override ;
    void SetInfo(QString name, QString head, QString msg);
    void SetInfo(std::shared_ptr<AuthInfo> auth_info);
    void SetInfo(std::shared_ptr<AuthRsp> auth_rsp);
    void ShowRedPoint(bool show);
    void UpdateWidChatTime(QString chat_time);
    void UpdateWidLastMsg(QString last_msg);
    // void SetSessionData(std::shared_ptr<ChatSessionData> chat_session_data);
    // std::shared_ptr<ChatSessionData> GetChatSessionData();
private:
    QString _name;
    QString _head;
    QString _last_msg;
    QString _last_chat_time;
    // std::shared_ptr<ChatSessionData> _chat_session_data;     //有item->setData(Qt::UserRole, uid);  或许就不需要这个了
    Ui::ChatUserWid *ui;
};

#endif // CHATUSERWID_H
