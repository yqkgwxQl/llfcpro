#include "chatdialog.h"
#include "ui_chatdialog.h"
#include <QRandomGenerator>
#include <chatuserwid.h>
#include <QMovie>
#include <QTimer>
#include "statewidget.h"
#include <QMouseEvent>
#include "tcpmgr.h"
#include "usermgr.h"
#include "userdata.h"
#include <QJsonArray>
#include "conuseritem.h"

// #include "customizeedit.h"

ChatDialog::ChatDialog(QWidget *parent)
    : QDialog(parent), _fail_count(0), _min_friend_id(0), _loading_more(false), _has_more(true)
    , _mode(ChatUIMode::ChatMode),_state(ChatUIMode::ChatMode),ui(new Ui::ChatDialog){
    ui->setupUi(this);
    ui->add_btn->SetState("normal","hover","press");
    ui->search_edit->SetMaxLength(15);
    ui->search_edit->setPlaceholderText(QStringLiteral("搜索"));  //QStringLiteral : 编译时就生成好 QString 所需的内部数据，运行时零开销
    ShowSearch(false);
    //连接加载信号和槽
    connect(ui->chat_user_list,&ChatUserList::sig_loading_chat_user,this,&ChatDialog::slot_load_chat_user);
    // addChatUserList();//这些 item 不在 _chat_uid_to_items_map 里、UserRole 也没设

    QPixmap pixmap(":/res/head_1.jpg");
    QPixmap scale_pixmap = pixmap.scaled(ui->side_head_lb->size(),Qt::KeepAspectRatio);
    ui->side_head_lb->setPixmap(scale_pixmap);
    ui->side_head_lb->setScaledContents(true); // 告诉 QLabel，以后不管 label 尺寸怎么变，图片自动跟着拉伸填满。
    ui->side_chat_lb->setState("normal","hover","pressed","selected_normal","selected_hover","selected_pressed");
    ui->side_contact_lb->setState("normal","hover","pressed","selected_normal","selected_hover","selected_pressed");

    addLBGroup(ui->side_chat_lb);
    addLBGroup(ui->side_contact_lb);
    connect(ui->side_chat_lb, &StateWidget::clicked, this, &ChatDialog::slot_side_chat);
    connect(ui->side_contact_lb, &StateWidget::clicked, this, &ChatDialog::slot_side_contact);

    connect(ui->search_edit,&QLineEdit::textChanged,this,&ChatDialog::slot_text_changed);
    //为searchlist 设置search edit
    ui->search_list->setSearchEdit(ui->search_edit);
    // connect(ui->search_edit, &CustomizeEdit::sig_clear_search, this, [this](){
    //     ShowSearch(false);
    // });

    //连接申请添加好友信号
    connect(TcpMgr::GetInstance().get(),&TcpMgr::sig_friend_apply,this,&ChatDialog::slot_apply_friend);
    //连接认证添加好友信号
    connect(TcpMgr::GetInstance().get(),&TcpMgr::sig_add_auth_friend,this,&ChatDialog::slot_add_auth_friend);
    //链接自己认证回复信号
    connect(TcpMgr::GetInstance().get(),&TcpMgr::sig_auth_rsp,this,&ChatDialog::slot_auth_rsp);

    //连接加载获取friendlist的回复
    connect(TcpMgr::GetInstance().get(), &TcpMgr::sig_friendlist_rsp, this, &ChatDialog::slot_friendlist_rsp);
    //连接searchlist组件万一搜索到已经拥有的好友，即跳转到对应的聊天界面
    connect(ui->search_list, &SearchList::sig_jump_chat_item_by_search, this, &ChatDialog::slot_jump_chat_item_by_search);
    connect(ui->chat_user_list, &ChatUserList::sig_jump_chat_item_by_click, this, &ChatDialog::slot_jump_chat_item_by_click);
    //连接  tcp收到对端发送给我的文本消息的通知信号， 用于通知chat_dialog已经收到对端发送文本消息了
    connect(TcpMgr::GetInstance().get(), &TcpMgr::sig_receive_text_chat_msg, this, &ChatDialog::slot_receive_text_chat_msg);

    //chatdialog初始化时就发送加载自己好友的请求，获取自己的好友列表
    loadMoreFriend();
    this->installEventFilter(this);
    ui->side_chat_lb->setSelected(true);
}

ChatDialog::~ChatDialog(){
    delete ui;
}

void ChatDialog::slot_load_chat_user(){
    // if(!_b_loading){
    //     return;
    // }
    // _b_loading = true;
    // LoadingDialog* loading_dlg = new LoadingDialog(this);
    // loading_dlg->setModal(true);
    // loading_dlg->show();
    QLabel *loading_item = new QLabel(this);
    QMovie *movie = new QMovie(":/res/loading.gif");

    loading_item->setMovie(movie);
    loading_item->setFixedSize(250, 70);
    loading_item->setAlignment(Qt::AlignCenter);
    movie->setScaledSize(QSize(50, 50));

    QListWidgetItem *item = new QListWidgetItem;
    item->setSizeHint(QSize(250, 70));
    ui->chat_user_list->addItem(item);
    ui->chat_user_list->setItemWidget(item, loading_item);
    movie->start();
    qDebug() << "add new data to list.....";
    QApplication::processEvents();
    // QTimer::singleShot(1000, this, [this, item]() {
    //     qDebug() << "add new Data to list...";
    //     addChatUserList();
    //     ui->chat_user_list->takeItem(ui->chat_user_list->row(item));
    //     ui->chat_user_list->update();
    // });
    qDebug() << "add new Data to list...";
    // addChatUserList();
    loadMoreFriend();
    ui->chat_user_list->takeItem(ui->chat_user_list->row(item));
    ui->chat_user_list->update();
    //addChatUserList 本身很快 loading_dlg绘制完，立即销毁，根本看不到
    // addChatUserList();
    // loading_dlg->deleteLater();
    // _b_loading = false;
}

void ChatDialog::slot_side_chat(){
    qDebug()<< "receive side chat clicked";
    clearLabelState(ui->side_chat_lb);  //除了ui->side_chat_lb，剩下的侧边框都要变灰
    ui->stackedWidget->setCurrentWidget(ui->chat_page);
    _state = ChatUIMode::ChatMode;
    ShowSearch(false);
}

void ChatDialog::slot_side_contact(){
    qDebug()<< "receive side contact clicked";
    clearLabelState(ui->side_contact_lb);
    ui->stackedWidget->setCurrentWidget(ui->friend_apply_page);
    _state = ChatUIMode::ContactMode;
    ShowSearch(false);
}

void ChatDialog::slot_text_changed(const QString &text){
    if(!text.isEmpty()){
        ShowSearch(true);
    }
}

void ChatDialog::slot_apply_friend(std::shared_ptr<AddFriendApply> add_friend_apply){
    qDebug() << "receive apply friend slot, applyuid is " << add_friend_apply->_from_uid << " name is "
             << add_friend_apply->_name << " desc is " << add_friend_apply->_desc;
    //  // 去重：这个人已经申请过就直接忽略 ——————> 万一同一个人发了两次
    // bool is_already = UserMgr::GetInstance()->AlreadyApply(add_friend_apply->_from_uid);
    // if(is_already)
    //     return;
    UserMgr::GetInstance()->AddToApplyList(std::make_shared<ApplyInfo>(add_friend_apply));
    ui->side_contact_lb->showRedPoint(true);
    ui->con_user_list->ShowRedPoint(true);
    ui->friend_apply_page->AddNewApply(add_friend_apply);
}

//这个是针对对端同意我给它的申请，我的chat_user_list(就是此函数中进行变化)和con_user_list会变化(ContactUserList::slot_add_auth_rsp()中会变化)
void ChatDialog::slot_add_auth_friend(std::shared_ptr<AuthInfo> auth_info){
    qDebug() << "ChatDialog::slot_add_auth_friend";
    bool isHasFriend = UserMgr::GetInstance()->AreadyFriend(auth_info->_uid);
    if(isHasFriend){
        qDebug() << "ChatDialog::slot_add_auth_friend:has friend, not need add_auth_friend";
        return;
    }
    int randomValue = QRandomGenerator::global()->bounded(100);
    int msg_i = randomValue%strs.size();
    auto chat_session_data = UserMgr::GetInstance()->GetChatSessionByUid(auth_info->_uid);
    addOneFriendOnChatUserList(auth_info->_uid, auth_info->_name, auth_info->_icon, strs[msg_i]);
}

//这个是针对我本身同意对端给我的申请，我的chat_user_list(就是此函数中进行变化)和con_user_list会变化(ContactUserList::slot_auth_rsp()中会变化)
void ChatDialog::slot_auth_rsp(std::shared_ptr<AuthRsp> auth_rsp){
    qDebug() << "ChatDialog::slot_auth_rsp";
    // bool isHasFriend = UserMgr::GetInstance()->AreadyFriend(auth_rsp->_uid);
    // if(isHasFriend){
    //     qDebug() << "ChatDialog::slot_auth_rsp():has friend, not need add_auth_friend";
    //     return;
    // }
    int randomValue = QRandomGenerator::global()->bounded(100);     //？？？这里需等到last_msg的逻辑写完，然后需要修改这里
    int msg_i = randomValue%strs.size();
    addOneFriendOnChatUserList(auth_rsp->_uid, auth_rsp->_name, auth_rsp->_icon, strs[msg_i]);
    //取消红点显示
    ui->side_contact_lb->showRedPoint(false);
}

void ChatDialog::slot_friendlist_rsp(const QJsonObject &obj){
    _loading_more = false;
    if(obj["error"].toInt()){
        qDebug() << "friend_list rsp error:" << obj["error"].toInt();
        if(++_fail_count >= 3){
            _has_more = false;
        }
        return;     //失败了我就不往下进行，等待下次滚轮下滑到底部，发送信号触发ChatDialog::loadMoreFriend事件，添加好友
    }
    _fail_count = 0;

    int randomValue = QRandomGenerator::global()->bounded(100);
    const QJsonArray arr = obj["friend_list"].toArray();
    for(const auto& element : arr){
        QJsonObject obj_2 = element.toObject();
        qint64 id = static_cast<qint64>(obj_2["id"].toDouble());
        if(_min_friend_id == 0 || _min_friend_id > id)
            _min_friend_id = id;
        int msg_i = randomValue%strs.size();
        addOneFriendOnChatUserList(obj_2["uid"].toInt(), obj_2["name"].toString(), obj_2["icon"].toString(), strs[msg_i]);
    }
    _has_more = obj["has_more"].toBool() && !arr.empty();

    // 只在本机还没有当前会话时才自动选第一条，否则翻页会把用户的选中抢走
    if(_cur_chat_uid == 0 && ui->chat_user_list->count() > 0){
        selectFirstChatItem();
        showChatPageByUid(_cur_chat_uid);
    }
}

void ChatDialog::slot_jump_chat_item_by_search(std::shared_ptr<SearchInfo> si){
    if(!si)
        return;
    // 1. 退出搜索模式：清空输入框 + 切到会话页(slot_side_chat 内部已含 ShowSearch(false))
    ui->search_edit->clear();
    // ShowSearch(false);
    ui->side_chat_lb->setSelected(true);    //万一ShowSearch的前面的页面不是chat_page不就出问题了吗？？？
    slot_side_chat();

    // if(!_chat_uid_to_items_map.contains(si->_uid)){
    //     qDebug() << "用户已有此朋友(uid:" << si->_uid <<")对应的好友，但是客户端缓存并未加载此朋友，现在立即进行加载";
    //     addOneFriendOnChatUserList(si->_uid, si->_name, si->_icon, QString());  //传空串，这里以后一定要改进？？？
    //     UserMgr::GetInstance()->AddToFriendMap(si);
    // }

    // 2. 保证条目存在（服务端已判定是好友，本地没加载到就补一条）
    QListWidgetItem* item = _chat_uid_to_items_map.value(si->_uid, nullptr);
    if(item == nullptr){
        qDebug() << "好友 uid" << si->_uid << "未加载到会话列表，就地补一条";
        item = addOneFriendOnChatUserList(si->_uid, si->_name, si->_icon, QString());
        if(item == nullptr){
            qDebug() << "补建会话条目失败, uid =" << si->_uid;
            return;
        }
    }
    UserMgr::GetInstance()->AddToFriendMap(si);
    ui->chat_user_list->scrollToItem(item);
    selectChatItemByUid(si->_uid);  //这个函数就是 选中item而已
    showChatPageByUid(si->_uid);    //这个函数就是 展开对应uid的聊天界面
}

void ChatDialog::slot_jump_chat_item_by_click(std::shared_ptr<UserInfo> friend_info){
    if(!friend_info)
        return;

    QListWidgetItem* item = _chat_uid_to_items_map.value(friend_info->_uid, nullptr);
    ui->chat_user_list->scrollToItem(item);
    selectChatItemByUid(friend_info->_uid);  //这个函数就是 选中item而已
    showChatPageByUid(friend_info->_uid);    //这个函数就是 展开对应uid的聊天界面
}

void ChatDialog::slot_receive_text_chat_msg(int list_item_uid){
    auto iter = _chat_uid_to_items_map.find(list_item_uid);
    if(iter == _chat_uid_to_items_map.end() || iter.value() == nullptr){
        qDebug() << "未能找到uid(" << list_item_uid << ")对应的list_item, 或对应的list_item为空";
        return;
    }
    auto list_item = iter.value();
    //    std::shared_ptr<QWidget> w = std::make_shared<QWidget>(ui->chat_user_list->itemWidget(list_item));
    // std::shared_ptr<ChatUserWid> chat_user_wid = std::dynamic_pointer_cast<ChatUserWid>(w);
    /* 这里是有问题的--------------
     * 因为 QWidget 有个构造函数是 QWidget(QWidget* parent, ...)——make_shared<QWidget>(某个widget指针) 会被解析成"以那个 widget 为父窗口，
     * 新建一个空 QWidget"，编译完全没问题。
    但它做的事完全错了：新建的是一个普通 QWidget，不是原来那个 ChatUserWid；*/
    //取出此list_item上面的包装器(chat_user_wid)
    QWidget* w = ui->chat_user_list->itemWidget(list_item);
    ChatUserWid* chat_user_wid = qobject_cast<ChatUserWid*>(w);
    if(!chat_user_wid){
        qDebug() << "uid(" << list_item_uid << ") 的 itemWidget 不是 ChatUserWid，跳过";
        return;
    }
    auto chat_session_data = UserMgr::GetInstance()->GetChatSessionByUid(list_item_uid);
    if(!chat_session_data)
        return;
    chat_user_wid->ShowRedPoint(true);
    chat_user_wid->UpdateWidChatTime(chat_session_data->GetLastMsgTime());
    chat_user_wid->UpdateWidLastMsg(chat_session_data->GetLastMsgContent());

    // 如果当前正在和这个人聊天，右侧立刻重绘
    if(ui->chat_page->GetCurChatUid() == list_item_uid){
        ui->chat_page->SetChatUser(list_item_uid);
    }
}


bool ChatDialog::eventFilter(QObject *watched, QEvent *event){
    if(event->type() == QEvent::MouseButtonPress){
        QMouseEvent* mouse_event = dynamic_cast<QMouseEvent*>(event);
        if(_mode == ChatUIMode::SearchMode){
            // 将鼠标点击位置转换为搜索列表坐标系中的位置
            QPoint pos_in_searchlist = ui->search_list->mapFromGlobal(mouse_event->globalPos());
            // 判断点击位置是否在聊天列表的范围内
            if(!ui->search_list->rect().contains(pos_in_searchlist)){
                // 如果不在聊天列表内，清空输入框
                ui->search_edit->clear();
                ShowSearch(false);
            }
        }
    }
    return QDialog::eventFilter(watched, event);
}

void ChatDialog::ShowSearch(bool bsearch){
    if(bsearch){
        ui->chat_user_list->hide();
        ui->con_user_list->hide();
        ui->search_list->show();
        _mode = ChatUIMode::SearchMode;
    }else{
        if(_state == ChatUIMode::ChatMode){
            ui->search_list->hide();
            ui->con_user_list->hide();
            ui->chat_user_list->show();
        }else if(_state == ChatUIMode::ContactMode){
            ui->search_list->hide();
            ui->chat_user_list->hide();
            ui->con_user_list->show();
        }
    }
}

void ChatDialog::fflushPage(){
    ui->chat_user_list->clear();
    _chat_uid_to_items_map.clear();
    _fail_count = 0;
    _min_friend_id = 0;
    _loading_more = false;
    _has_more = true;
    loadMoreFriend();
}

void ChatDialog::loadMoreFriend(){
    if(_loading_more || !_has_more)
        return;
    _loading_more = true;
    QJsonObject obj;
    obj["uid"] = UserMgr::GetInstance()->GetUid();
    obj["begin"] = _min_friend_id;
    obj["limit"] = 10;

    emit TcpMgr::GetInstance()->sig_send_data(ReqId::ID_GET_FRIEND_LIST_REQ, QJsonDocument(obj).toJson(QJsonDocument::Compact));
}

QListWidgetItem* ChatDialog::addOneFriendOnChatUserList(int uid, const QString& name, const QString& head, const QString& msg){
    auto iter = _chat_uid_to_items_map.find(uid);
    if(iter != _chat_uid_to_items_map.end()){
        qDebug() << "已有此uid(" << uid << ")对应的朋友，添加无效";
        return iter.value();
    }

    auto* chat_user_wid = new ChatUserWid();
    chat_user_wid->SetInfo(name, head, msg);
    QListWidgetItem* item = new QListWidgetItem();
    item->setSizeHint(chat_user_wid->sizeHint());

    item->setData(Qt::UserRole, uid);
    /*setData(role, value) 是 QListWidgetItem 的**"按角色存一个值"** 的接口，Qt::UserRole 是 Qt 专门留给应用程序自己用的角色号 ——
     *                                                  相当于给这个列表条目挂一个自定义字段，条目的生命周期就是它的生命周期。
        1. 它到底存在哪
        QListWidgetItem 内部就是一个 QMap<int role, QVariant> 式的数据槽。setData 往槽里写，data 从槽里读：

            item->setData(Qt::UserRole, uid);               写
            int uid = item->data(Qt::UserRole).toInt();     读
        2.同理实现
            item->setSizeHint(chat_user_wid->sizeHint());
            item->setText("abc");      // == setData(Qt::DisplayRole,   QString("abc"))
            item->setIcon(icon);      // == setData(Qt::DecorationRole, icon)           */


    /*  ui->chat_user_list->setItemWidget(item, chat_user_wid);
     *  1.一句话说明：
     *      setItemWidget(item, widget) = 把一个 QWidget 绑定到某个 item 的矩形区域上，作为这一行的"皮肤"。item 本身没有被替换、
     *          没有被覆盖掉，它依然完整存在 —— 只是它自己不再负责"长什么样"。
     *  2. 简要理解：
            模型层	QListWidgetItem	占一行、有 sizeHint、有 flags、有 data(UserRole)、参与选中/焦点/滚动/点击
            视图层	ChatUserWid	只负责"这一行画成什么样"（头像、名字、最后一条消息）
    */
    //使用addItem()会添加到底部
    // ui->chat_user_list->addItem(item);
    //需要使用insertItem() 添加成第一个
    ui->chat_user_list->insertItem(0, item);
    ui->chat_user_list->setItemWidget(item, chat_user_wid);

    _chat_uid_to_items_map.insert(uid, item);
    qDebug() << "已添加一个friend到chat_user_list里";
    return item;
}

void ChatDialog::selectChatItemByUid(int uid){
    _cur_chat_uid = 0;
    if(ui->chat_user_list->count() <= 0){
        qDebug() << "ui->chat_user_list->count() <= 0，无法进行选中";
        return;
    }

    auto iter = _chat_uid_to_items_map.find(uid);
    if(iter == _chat_uid_to_items_map.end() || iter.value() == nullptr){
        qDebug() << "uid(" << uid << ")对应的chat_list_item not found" ;
        return;
    }

    ui->chat_user_list->setCurrentItem(iter.value());
    _cur_chat_uid = uid;
    qDebug() << "已选中uid(" << uid << ")对应的chat_list_item";
}

void ChatDialog::selectFirstChatItem(){
    _cur_chat_uid = 0;
    if(ui->chat_user_list->count() <= 0){
        qDebug() << "ui->chat_user_list->count() <= 0，无法进行选中";
        return;
    }

    auto first = ui->chat_user_list->item(0);
    if(!first)
        return; //这种情况不可能发生
    ui->chat_user_list->setCurrentItem(first);
    _cur_chat_uid = first->data(Qt::UserRole).toInt();
    qDebug() << "已经选择第一个chat_list_item";
}

void ChatDialog::showChatPageByUid(int uid){
    // if (ui->chat_user_list->count() <= 0) {
    //     qDebug() << "ui->chat_user_list->count() <= 0，无法showChatPageByUid";
    //     return;
    // }

    // auto iter = _chat_uid_to_items_map.find(uid);
    // if (iter == _chat_uid_to_items_map.end() || iter.value() == nullptr) {
    //     qDebug() << "showChatPageByUid: uid" << uid << "not found";
    //     return;
    // }
    // auto item = iter.value();
    // if (!item) {
    //     return;
    // }

    // QWidget *widget = ui->chat_user_list->itemWidget(item);   // 条目可能已从列表移除
    // if (!widget) {
    //     qDebug() << "QWidget *widget = ui->chat_user_list->itemWidget(item); error";
    //     return;
    // }
    // auto customItem = qobject_cast<ListItemBase*>(widget);
    // if (!customItem) {
    //     qDebug() << "auto customItem = qobject_cast<ListItemBase*>(widget); error";
    //     return;
    // }
    // auto itemType = customItem->GetItemType();
    // if (itemType != CHAT_USER_ITEM) {
    //     qDebug() << "showChatPageByUid: item type is not CHAT_USER_ITEM";
    //     return;
    // }
    // auto chat_wid = qobject_cast<ChatUserWid*>(customItem);
    // if (!chat_wid) {
    //     qDebug() << "auto chat_wid = qobject_cast<ChatUserWid*>(customItem); error";
    //     return;
    // }

    // // TODO: 你的 ChatPage 目前没有会话数据接口，等补上之后放开下面这行
    // // ui->chat_page->SetChatUser(chat_wid->GetUid());

    if (ui->chat_user_list->count() <= 0) {
        qDebug() << "ui->chat_user_list->count() <= 0，无法showChatPageByUid";
        return;
    }
    auto iter = _chat_uid_to_items_map.find(uid);
    if (iter == _chat_uid_to_items_map.end() || iter.value() == nullptr) {
        qDebug() << "showChatPageByUid: uid" << uid << "not found";
        return;
    }

    // 右侧聊天页换成这个人的会话（标题 + 历史气泡）
    ui->chat_page->SetChatUser(uid);
}

//-----------------用于测试chat_user_wid----------------------------


void ChatDialog::addChatUserList(){
    // 创建QListWidgetItem，并设置自定义的widget
    for(int i = 0; i < 13; i++){
        int randomValue = QRandomGenerator::global()->bounded(100); // 生成0到99之间的随机整数
        int str_i = randomValue%strs.size();
        int head_i = randomValue%heads.size();
        int name_i = randomValue%names.size();

        auto *chat_user_wid = new ChatUserWid();
        chat_user_wid->SetInfo(names[name_i], heads[head_i], strs[str_i]);
        QListWidgetItem *item = new QListWidgetItem;
        // qDebug()<<"chat_user_wid sizeHint is " << chat_user_wid->sizeHint();
        item->setSizeHint(chat_user_wid->sizeHint());
        ui->chat_user_list->addItem(item);
        ui->chat_user_list->setItemWidget(item, chat_user_wid);
    }
}

void ChatDialog::clearLabelState(StateWidget *cur_state_widget){
    for(auto& state_widget:_lb_list){
        if(state_widget == cur_state_widget)
            continue;
        state_widget->clearState();
    }
}

void ChatDialog::addLBGroup(StateWidget * state_widget){
    _lb_list.push_back(state_widget);
}
//-----------------用于测试chat_user_wid----------------------------