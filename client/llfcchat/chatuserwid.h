#ifndef CHATUSERWID_H
#define CHATUSERWID_H

#include <QWidget>
#include "listitembase.h"
#include <QSize>

namespace Ui {
class ChatUserWid;
}

class ChatUserWid : public ListItemBase
{
    Q_OBJECT

public:
    explicit ChatUserWid(QWidget *parent = nullptr);
    ~ChatUserWid();
    QSize sizeHint()const override ;
    void SetInfo(QString name, QString head, QString msg);
private:
    QString _name;
    QString _head;
    QString _msg;
    Ui::ChatUserWid *ui;
};

#endif // CHATUSERWID_H
