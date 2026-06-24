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
private:
    ChatRole _role;
    QLabel* _pNameLabel;
    QLabel* _pIconLabel;
    QWidget* _pBubble;
};

#endif // CHATITEMBASE_H
