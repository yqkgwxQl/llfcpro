#ifndef CHATPAGE_H
#define CHATPAGE_H

#include <QWidget>

class UserMgr;
class ChatItemBase;
class ChatDataBase;
class TextBubble;
class PictureBubble;
class Batch;
enum class MsgStatus;

namespace Ui {
class ChatPage;
}

class ChatPage : public QWidget
{
    Q_OBJECT

public:
    explicit ChatPage(QWidget *parent = nullptr);
    ~ChatPage();
    void refreshChatView();
    void SetChatUser(int other_uid);
    int GetCurChatUid();
    void clearItems();
protected:
    virtual void paintEvent(QPaintEvent *event)override;
private slots:
    void on_send_btn_clicked();
    void slot_updata_msg_status(int other_uid, std::shared_ptr<QVector<QString>> unique_ids_vector_ptr, MsgStatus status);
    void slot_offline_msg_rsp(int other_uid, bool has_more, qint64 max_id);
private:
    void AppendChatMsg(std::shared_ptr<ChatDataBase> msg);  //渲染一条消息气泡

    Ui::ChatPage *ui;
    int _cur_chat_uid;
    // QMap<QString, std::shared_ptr<ChatItemBase>> _unrsp_items;  //还未收到服务器回执的气泡
    QMap<QString, ChatItemBase*> _unrsp_items;  //还未收到服务器回执的气泡

    //拉取远端离线消息
    void loadOfflineMsgMore();
    // int _fail_count;
    qint64 _max_msg_id;
    bool _loading_more;
    bool _has_more;
};

#endif // CHATPAGE_H
