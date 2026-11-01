#ifndef CHATITEMBASE_H
#define CHATITEMBASE_H
#include <QWidget>
#include "global.h"
#include <QPixmap>
#include <QLabel>

class ChatItemBase: public QWidget
{
public:
    ChatItemBase(ChatRole role,QWidget* parent = nullptr);
    void setUserName(const QString& name);
    void setUserIcon(const QPixmap& icon);
    void setWidget(QWidget *w);
    void setStatus(MsgStatus status); //发送中/失败/已读 小图标，只对“自己发的”有意义
private:
    ChatRole _role;
    QLabel* _pNameLabel;
    QLabel* _pIconLabel;
    QWidget* _pBubble;
    QLabel* _pStatusLabel;
};

#endif // CHATITEMBASE_H
