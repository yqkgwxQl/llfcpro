#ifndef CHATUSERLIST_H
#define CHATUSERLIST_H
#include <QObject>
#include <QListWidget>

class UserInfo;
class ChatUserList:public QListWidget
{
    Q_OBJECT
public:
    ChatUserList(QWidget *parent = nullptr);
    ~ChatUserList();
public slots:
    void slot_item_clicked(QListWidgetItem* item);
signals:
    void sig_loading_chat_user();
    void sig_jump_chat_item_by_click(std::shared_ptr<UserInfo> friend_info);
protected:
    //bool eventFilter(QObject *object, QEvent *event) override;重写了这个
    /*相比clickedbtn.h更加灵活
    不用去找
    virtual void enterEvent(QEnterEvent *event) override;
    virtual void leaveEvent(QEvent *event) override;
    virtual void mousePressEvent(QMouseEvent *event) override;
    virtual void mouseReleaseEvent(QMouseEvent *event) override;
    */
    virtual bool eventFilter(QObject* watched,QEvent* event) override;
};

#endif // CHATUSERLIST_H
