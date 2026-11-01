#ifndef CHATPAGE_H
#define CHATPAGE_H

#include <QWidget>

class UserMgr;
class ChatItemBase;
class ChatDataBase;
class TextBubble;
class PictureBubble;

namespace Ui {
class ChatPage;
}

class ChatPage : public QWidget
{
    Q_OBJECT

public:
    explicit ChatPage(QWidget *parent = nullptr);
    ~ChatPage();

    void SetChatUser(int other_uid);
    int GetCurChatUid();
    void clearItems();
protected:
    virtual void paintEvent(QPaintEvent *event)override;
private slots:
    void on_send_btn_clicked();

private:
    void AppendChatMsg(std::shared_ptr<ChatDataBase> msg);  //渲染一条消息气泡

    Ui::ChatPage *ui;
    int _cur_chat_uid;
    // QMap<QString, std::shared_ptr<ChatItemBase>> _unrsp_items;  //还未收到服务器回执的气泡
    QMap<QString, ChatItemBase*> _unrsp_items;  //还未收到服务器回执的气泡
};

#endif // CHATPAGE_H
