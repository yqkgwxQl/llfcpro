#include "chatpage.h"
#include "ui_chatpage.h"
#include <QStyleOption>
#include <QPaintEvent>
#include <QPainter>
#include "chatitembase.h"
#include "textbubble.h"
#include "picturebubble.h"
#include "messagetextedit.h"
#include "usermgr.h"
#include "userdata.h"
#include "chatitembase.h"
#include <QDateTime>
#include <QUuid>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <tcpmgr.h>
#include "global.h"

ChatPage::ChatPage(QWidget *parent)
    : QWidget(parent), _cur_chat_uid(0), _max_msg_id(0), _loading_more(false), _has_more(true)
    , ui(new Ui::ChatPage)
{
    ui->setupUi(this);
    //设置按钮样式
    ui->receive_btn->SetState("normal","hover","press");
    ui->send_btn->SetState("normal","hover","press");
    //设置图标样式
    ui->emo_lb->SetState("normal","hover","press","normal","hover","press");
    ui->file_lb->SetState("normal","hover","press","normal","hover","press");
    // ChatPage 构造函数里加一行 (但是不需要，qt6中即是不重写QWidget的paintEvent，对于一些qss属性也是生效的）
    // ui->tool_wid->setStyleSheet("background:#ffffff; border-bottom:0.5px solid #ececec;");

    // 连接 Enter 发送信号
    connect(ui->chat_edit, &MessageTextEdit::send, this, &ChatPage::on_send_btn_clicked);
    // 连接 更新信息状态信号
    connect(TcpMgr::GetInstance().get(), &TcpMgr::sig_update_msg_status, this, &ChatPage::slot_updata_msg_status);
    //tcp连接断开
        /*auto& [uniqeu_id, item] 是结构化绑定，它要求右边的东西是“可以拆开的类型”——比如 std::pair、std::tuple、数组、或者有公开成员的结构体。
            而 Qt 容器（QMap / QHash）的迭代器，operator* 返回的是 value 本身，不是 std::pair：
                ~~~
                    // Qt 的 QMap/QHash 迭代器语义
                    *it  →  ChatItemBase*        // 只有值，没有 key
                    it.key()   // 才是 key
                ~~~
        所以你那句 for (auto& [uid, item] : _unrsp_items) 实际是在拆一个 ChatItemBase* → 编译器就报这个错。
        这和 std::map 不一样：STL 的迭代器解引用得到 std::pair<const K, V>，可以拆；Qt 的容器不行。*/
    connect(TcpMgr::GetInstance().get(), &TcpMgr::sig_connection_closed, this, [this](){
        for(auto iter = _unrsp_items.begin(); iter != _unrsp_items.end(); iter++){
            if(iter.value())
                iter.value()->setStatus(MsgStatus::FAILED);
        }
        if(_cur_chat_uid > 0){
            auto chat_session_data = UserMgr::GetInstance()->GetChatSessionByUid(_cur_chat_uid);
            if(chat_session_data){
                for(auto& chat_data_base : chat_session_data->GetMsgList()){
                    if(chat_data_base && chat_data_base->GetStatus() == MsgStatus::SENDING)
                        chat_session_data->UpdateMsgStatus(chat_data_base->GetUniqueId(), MsgStatus::FAILED);
                }
            }
        }
    });
    connect(TcpMgr::GetInstance().get(), &TcpMgr::sig_offline_msg_rsp, this, &ChatPage::slot_offline_msg_rsp);
}

ChatPage::~ChatPage()
{
    delete ui;
}

void ChatPage::refreshChatView(){
    //在clearItems()之前，否则会显示空白，无法再次AppendChatMsg()显示新气泡了
    std::shared_ptr<ChatSessionData> chat_session_data = UserMgr::GetInstance()->GetChatSessionByUid(_cur_chat_uid);
    if(!chat_session_data)
        return;

    clearItems();

    auto msg_list = chat_session_data->GetMsgList();
    for(auto& msg : msg_list){
        AppendChatMsg(msg);
    }
}

void ChatPage::SetChatUser(int other_uid){
    if(_cur_chat_uid == other_uid)
        return;
    std::shared_ptr<ChatSessionData> chat_session_data = UserMgr::GetInstance()->GetChatSessionByUid(other_uid);
    if(!chat_session_data)
        return;

    clearItems();
    _cur_chat_uid = other_uid;
    std::shared_ptr<UserInfo> friend_user_info = UserMgr::GetInstance()->GetFriendInfoByUid(other_uid);
    if(friend_user_info){
        ui->title_lb->setText(friend_user_info->_name);
    }else{
        qDebug() << "ChatPage::SetChatUser: 本地没有 uid" << other_uid << "的资料";
        ui->title_lb->setText(QString::number(other_uid));
    }

    _max_msg_id   = 0;
    _has_more     = true;
    _loading_more = false;
    loadOfflineMsgMore();

    auto msg_list = chat_session_data->GetMsgList();
    for(auto& msg : msg_list){
        AppendChatMsg(msg);
    }

}

int ChatPage::GetCurChatUid(){
    return _cur_chat_uid;
}

void ChatPage::clearItems(){
    ui->chat_data_list->removeAllItem();
    _unrsp_items.clear();       // _unrsp_items ————> 用来保存本会话的未收到回报的chat_item_base，切换会话则表示切换的会话的未收到回报的chat_itme_base
                                // 这是通过AppendChatMsg()根据其status做到的，很巧妙
}

/*ChatPage 的 paintEvent 对谁起作用？      只对 ChatPage 自己身上的 QSS 起作用
 对子控件有影响吗？                          没有，每个子控件自己画自己的
该不该重写？                                  该，这是标准写法，保证任何 Qt 版本、任何 QSS 属性都正常生效*/
//if不重写paintEvent的话 QWidget不会使你的ChatPage自身的qss生效 (子控件if有QWidget也需要重写才能对自己的qss生效)，
        //反之继承自QWidget的QLabel和QPushButton的paintEvent会自动重写，能够使你的qss生效。而自定义的继承自QWidget的控件must重写paintEvent才能使qss生效
//注意：这里的子控件和父空间的唯一关联就是，子控件是安装于父控件上的
    //而对于eg:继承chatpage的对象，由于基类chatpage是重写过paintEvent的，因此这个继承chatpage的子类对象——它的qss是有效的
/*eg:  在qss中添加          ————>     这个例子可以看出重写paintEvent的必要性
ChatPage {
    background-color: rgb(200, 200, 200);
}*/
void ChatPage::paintEvent(QPaintEvent *event){
    QStyleOption opt;            // ① 创建一个样式选项对象
    opt.initFrom(this);              // ② 用当前控件的信息初始化它（大小、状态等）

    QPainter p(this);            // ③ 创建一个画家，画在当前控件上

    //它告诉 Qt 的样式系统："请按照当前主题和 QSS 规则，把这个控件画出来。"
    style()->drawPrimitive(      // ④ 调用当前风格的绘制方法
        QStyle::PE_Widget,       //    绘制"Widget"图元
        &opt,                    //    传入样式选项
        &p,                      //    传入画家
        this                     //    传入控件本身
        );
}
void ChatPage::on_send_btn_clicked(){
    if(_cur_chat_uid <= 0){
        qDebug() << "ChatPage::on_send_btn_clicked: 当前没有选中会话，不能发送";
        return;
    }

    auto pTextEdit = ui->chat_edit;
    //注意：getMsgList() 内部已经 this->clear()了，这里不用再清输入框
    const QVector<MsgInfo>& msg_list = pTextEdit->getMsgList();
    if(msg_list.isEmpty())
        return;

    QString self_name = UserMgr::GetInstance()->GetName();
    QString self_icon = UserMgr::GetInstance()->GetIcon();
    int self_uid = UserMgr::GetInstance()->GetUid();
    std::shared_ptr<ChatSessionData> chat_session_data = UserMgr::GetInstance()->GetChatSessionByUid(_cur_chat_uid);

    int send_size = 0;
    QJsonObject sendObj;
    QJsonArray text_array;

    auto fflush = [&text_array, this, &send_size](){
        if(text_array.isEmpty())
            return;
        QJsonObject obj;
        //sendObj["from_uid"] = self_uid;  //from_uid就是自己，直接从连接的服务器里拿到from_uid，防止中间人伪造
        obj["to_uid"] = _cur_chat_uid;
        obj["text_array"] = text_array;
        emit TcpMgr::GetInstance()->sig_send_data(
            ReqId::ID_TEXT_CHAT_MSG_REQ, QJsonDocument(obj).toJson(QJsonDocument::Compact));

        text_array = QJsonArray();
        send_size = 0;
    };
    for(auto& one : msg_list){
        //消息内容长度不合规就跳过
        const int bytes = one.content.toUtf8().size();  //按字节来算，而非字符（one.content.length())
        if(bytes > 1024){
            qDebug() << "消息长度太长了，进行丢弃";
            continue;
        }

        // std::shared_ptr<QWidget> pBubble = nullptr;
        ChatMsgType msg_type = ChatMsgType::TEXT;
        if(one.msgFlag == "text"){
            // pBubble = std::make_shared<TextBubble>(ChatRole::Self, one.content);
            msg_type = ChatMsgType::TEXT;
        }
        else if(one.msgFlag == "image"){
            // pBubble = std::make_shared<PictureBubble>(ChatRole::Self, QPixmap(one.content));
            msg_type = ChatMsgType::PIC;
        }
        else if(one.msgFlag == "file"){
            msg_type = ChatMsgType::FILE;
        }
        // if(!pBubble)
        //     continue;

        const QString chat_time = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss");
        const QString msg_unique_id = QUuid::createUuid().toString(QUuid::WithoutBraces);
        std::shared_ptr<ChatDataBase> chat_data_base;
        if(msg_type == ChatMsgType::TEXT)
            chat_data_base = std::make_shared<TextChatData>(msg_unique_id, self_uid, one.content, MsgStatus::SENDING, chat_time);
        else
            chat_data_base = std::make_shared<ChatDataBase>(msg_unique_id, self_uid, msg_type, one.content, MsgStatus::SENDING, chat_time);

        //先落地本地缓存和运行缓存
        auto appendmsg_code = chat_session_data->AppendMsg(msg_unique_id, chat_data_base, true);
        if(appendmsg_code == AppendMsgCode::PERSIST_FAILED){
            qDebug() << "在发送消息之前，将消息落地本地缓存和运行缓存里失败, 此消息发送异常, 丢弃";   //？？？这里应该有提示的
            continue;
        }
        //然后再添加气泡
        AppendChatMsg(chat_data_base);
        //发送数据------------
        if(msg_type == ChatMsgType::TEXT){
            if(send_size + bytes> 1024)
                fflush();
            send_size += bytes;
            QJsonObject msgObj;
            msgObj["unique_id"] = msg_unique_id;
            msgObj["content"] = one.content;
            msgObj["chat_time"] = chat_time;
            text_array.append(msgObj);
        }else{
            //....
        }
    }
    //直到for循环结束send_size都未 > 1024
    fflush();
}

void ChatPage::slot_updata_msg_status(int other_uid, std::shared_ptr<QVector<QString>> unique_ids_vector_ptr, MsgStatus status){
    if(_cur_chat_uid != other_uid)
        return;
    for(const QString& unique_id : *unique_ids_vector_ptr){
        auto iter = _unrsp_items.find(unique_id);
        if(iter != _unrsp_items.end()){
            iter.value()->setStatus(status);
            _unrsp_items.remove(unique_id);
        }
    }
}

//这里使用SetChatUser虽然是对的，但是感觉破坏了SetChatUser的完整性，让SetChatUser相当于有刷新界面的功能
void ChatPage::slot_offline_msg_rsp(int other_uid, bool has_more, qint64 max_id){
    if(_cur_chat_uid == other_uid){
        // SetChatUser(other_uid);
        refreshChatView();

        //移进去是为了，不能让旧对话缓存污染新对话缓存
        _loading_more = false;
        _has_more =  has_more;
        if(!has_more){
            qDebug() << "没有更多离线消息";
            return;
        }
        _max_msg_id = max_id;
        loadOfflineMsgMore();
    }

}

void ChatPage::AppendChatMsg(std::shared_ptr<ChatDataBase> msg){
    if(!msg)
        return;
    const ChatRole msg_role = msg->GetSendUid() == UserMgr::GetInstance()->GetUid() ? ChatRole::Self : ChatRole::Other;
    QString name;
    QString icon;
    if(msg_role == ChatRole::Self){
        name = UserMgr::GetInstance()->GetName();
        icon = UserMgr::GetInstance()->GetIcon();
    }else{
        std::shared_ptr<UserInfo> friend_user_info = UserMgr::GetInstance()->GetFriendInfoByUid(msg->GetSendUid());
        if(!friend_user_info){  //这里不会有问题吗？万一缓存中没有呢？？？
            qDebug() << "并未查到你有任何friend_uid == " << msg->GetSendUid() << " 的用户，无法void ChatPage::AppendChatMsg(std::shared_ptr<ChatDataBase> msg)";
            return;
        }
        name = friend_user_info->_name;
        icon = friend_user_info->_icon;
    }

    // std::shared_ptr<QWidget> pBubble = nullptr;
    QWidget* pBubble = nullptr;
    if(msg->GetMsgType() == ChatMsgType::TEXT){
        // pBubble = std::make_shared<TextBubble>(msg_role, msg->GetContent());    //隐式的子类转父类
        pBubble = new TextBubble(msg_role, msg->GetContent());
    }else if(msg->GetMsgType() == ChatMsgType::PIC){
        // pBubble = std::make_shared<PictureBubble>(msg_role, QPixmap(msg->GetContent()));      //隐式的子类转父类
        pBubble = new PictureBubble(msg_role, QPixmap(msg->GetContent()));
    }else if(msg->GetMsgType() == ChatMsgType::FILE){
        //这里暂时不做处理
    }else{

    }
    if(!pBubble)
        return;

    // std::shared_ptr<ChatItemBase> chat_item_base = std::make_shared<ChatItemBase>(msg_role);    //使用shared_ptr，引用计数归0，即消失，反倒不如交给qt去管理
    ChatItemBase* chat_item_base = new ChatItemBase(msg_role);
    chat_item_base->setUserName(name);
    chat_item_base->setUserIcon(icon);
    chat_item_base->setWidget(pBubble);
    //只有自己发的消息才显示状态图标
    if(msg_role == ChatRole::Self){
        chat_item_base->setStatus(msg->GetStatus());
    }

    ui->chat_data_list->appendItem(chat_item_base);     //if是shared_ptr的话，这里返回时，引用计数会-1
    //还没收到回执的气泡先记下来，等服务器回包后改状态
    if(msg_role == ChatRole::Self && msg->GetStatus() == MsgStatus::SENDING){
        _unrsp_items.insert(msg->GetUniqueId(), chat_item_base);
    }
}

void ChatPage::loadOfflineMsgMore(){
    if(_loading_more || !_has_more)
        return;
    _loading_more = true;
    QJsonObject obj;
    obj["other_uid"] = _cur_chat_uid;
    obj["after_id"] = _max_msg_id;
    obj["limit"] = 10;
    emit TcpMgr::GetInstance()->sig_send_data(ReqId::ID_GET_OFFLINE_MESSAGE_REQ, QJsonDocument(obj).toJson(QJsonDocument::Compact));
}

