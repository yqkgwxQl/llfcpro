#include "applyfriendpage.h"
#include "ui_applyfriendpage.h"
#include "tcpmgr.h"
#include <QRandomGenerator>
#include <QStyleOption>
#include <QPainter>
#include "usermgr.h"
#include "authenfriend.h"
#include <QJsonObject>
#include "usermgr.h"
#include <QJsonArray>

ApplyFriendPage::ApplyFriendPage(QWidget *parent) :
    QWidget(parent), _fail_count(0), _min_apply_id(0), _loading_more(false), _has_more(true), ui(new Ui::ApplyFriendPage)
{
    ui->setupUi(this);
    connect(ui->apply_friend_list, &ApplyFriendList::sig_show_search, this, &ApplyFriendPage::sig_show_search); //？？？？我发现没有任何处理函数
    // loadApplyList();
    //接收tcp传递的authrsp信号处理
    connect(TcpMgr::GetInstance().get(), &TcpMgr::sig_auth_rsp, this, &ApplyFriendPage::slot_auth_rsp);
    //ui->apply_friend_list传来加载更多apply_list的信号
    connect(ui->apply_friend_list, &ApplyFriendList::sig_loading_apply_list, this, &ApplyFriendPage::loadMoreApply);
    //接收tcp传递的sig_applylist_rsp
    connect(TcpMgr::GetInstance().get(), &TcpMgr::sig_applylist_rsp, this, &ApplyFriendPage::slot_applylist_rsp);

    loadMoreApply();        //向服务端发送需要得到apply_list的请求
    //提问? 为什么构造函数里可以调用成员函数? ———— 因为构造函数属于类，编译器就存在了；构造函数时构造对象时才进行调用
}

ApplyFriendPage::~ApplyFriendPage(){
    delete ui;
}

void ApplyFriendPage::AddNewApply(std::shared_ptr<AddFriendApply> apply){
    if(_unauth_items.count(apply->_from_uid)){
        qDebug() << "已有uid = " << apply->_from_uid << " 向本用户的申请，因此丢弃此apply";
        return;
    }
    //先模拟头像随机，以后头像资源增加资源服务器后再显示
    int randomValue = QRandomGenerator::global()->bounded(100); // 生成0到99之间的随机整数
    int head_i = randomValue % heads.size();
    auto* apply_item = new ApplyFriendItem();
    auto apply_info = std::make_shared<ApplyInfo>(apply->_from_uid,
                                                  apply->_name, apply->_desc,heads[head_i], apply->_name, apply->_sex, 0);
    apply_item->SetInfo(apply_info);
    QListWidgetItem* item = new QListWidgetItem;
    //qDebug()<<"chat_user_wid sizeHint is " << chat_user_wid->sizeHint();
    item->setSizeHint(apply_item->sizeHint());
    item->setFlags(item->flags() & ~Qt::ItemIsEnabled & ~Qt::ItemIsSelectable);
    ui->apply_friend_list->insertItem(0,item);
    ui->apply_friend_list->setItemWidget(item, apply_item);

    _unauth_items[apply->_from_uid] = apply_item;

    apply_item->ShowAddBtn(true);
    //收到审核好友信号
    connect(apply_item, &ApplyFriendItem::sig_auth_friend, [this](std::shared_ptr<ApplyInfo> apply_info) {
               auto* authFriend = new AuthenFriend(this);
               authFriend->setModal(true);
               authFriend->SetApplyInfo(apply_info);
               authFriend->show();
    });
}

void ApplyFriendPage::paintEvent(QPaintEvent *event){
    QStyleOption opt;
    opt.initFrom(this);
    QPainter p(this);
    style()->drawPrimitive(QStyle::PE_Widget, &opt, &p, this);
}

void ApplyFriendPage::loadApplyList(){
    //添加好友申请
    auto apply_list = UserMgr::GetInstance()->GetApplyList();
    for(auto &apply: apply_list){
        if(_unauth_items.count(apply->_uid))
            return;
        int randomValue = QRandomGenerator::global()->bounded(100); // 生成0到99之间的随机整数
        int head_i = randomValue % heads.size();        //这个是随机头像
        auto* apply_item = new ApplyFriendItem();
        apply->SetIcon(heads[head_i]);
        apply_item->SetInfo(apply);     // apply_list : vector<ApplyInfo> 一个以ApplyInfo这种数据结构为基础的数组
                                        // apply_item : 含有std::shared_ptr<ApplyInfo> _apply_info 的  条目
        QListWidgetItem* item = new QListWidgetItem;
        //qDebug()<<"chat_user_wid sizeHint is " << chat_user_wid->sizeHint();
        item->setSizeHint(apply_item->sizeHint());
        item->setFlags(item->flags() & ~Qt::ItemIsEnabled & ~Qt::ItemIsSelectable);
        ui->apply_friend_list->insertItem(0,item);
        ui->apply_friend_list->setItemWidget(item, apply_item);
        if(apply->_status){
            apply_item->ShowAddBtn(false);
        }else{
            apply_item->ShowAddBtn(true);
            auto uid = apply_item->GetUid();
            _unauth_items[uid] = apply_item;
        }

        //收到审核好友信号
        connect(apply_item, &ApplyFriendItem::sig_auth_friend, [this](std::shared_ptr<ApplyInfo> apply_info) {
                       auto* authFriend = new AuthenFriend(this);
                       authFriend->setModal(true);
                       authFriend->SetApplyInfo(apply_info);
                       authFriend->show();
        });
    }

    // 模拟假数据，创建QListWidgetItem，并设置自定义的widget
    for(int i = 0; i < 13; i++){
        int randomValue = QRandomGenerator::global()->bounded(100); // 生成0到99之间的随机整数
        int str_i = randomValue%strs.size();
        int head_i = randomValue%heads.size();
        int name_i = randomValue%names.size();

        auto *apply_item = new ApplyFriendItem();
        auto apply = std::make_shared<ApplyInfo>(0, names[name_i], strs[str_i],
                                                 heads[head_i], names[name_i], 0, 1);
        apply_item->SetInfo(apply);
        QListWidgetItem *item = new QListWidgetItem;
        //qDebug()<<"chat_user_wid sizeHint is " << chat_user_wid->sizeHint();
        item->setSizeHint(apply_item->sizeHint());
        item->setFlags(item->flags() & ~Qt::ItemIsEnabled & ~Qt::ItemIsSelectable);
        ui->apply_friend_list->addItem(item);
        ui->apply_friend_list->setItemWidget(item, apply_item);
        //收到审核好友信号
        connect(apply_item, &ApplyFriendItem::sig_auth_friend, [this](std::shared_ptr<ApplyInfo> apply_info){
            //            auto *authFriend =  new AuthenFriend(this);
            //            authFriend->setModal(true);
            //            authFriend->SetApplyInfo(apply_info);
            //            authFriend->show();
        });
    }
}

void ApplyFriendPage::loadMoreApply(){
    if(_loading_more || !_has_more) //if我要加载更多（这个加载更多并非第一次加载更多，而是向下滚动触发此时间后的第二次、第三次等等的加载更多
                //        or 我没有更多数据时，我要进行返回  ——> 防止传空包，浪费资源
        return;
    _loading_more = true;
    QJsonObject jsonObj;
    jsonObj["uid"] = UserMgr::GetInstance()->GetUid();
    jsonObj["begin"] = static_cast<long long>(_min_apply_id);
    jsonObj["limit"] = 10;

    emit TcpMgr::GetInstance()->sig_send_data(ReqId::ID_GET_APPLY_LIST_REQ, QJsonDocument(jsonObj).toJson(QJsonDocument::Compact));
}

void ApplyFriendPage::fflushPage(){
    // 清掉旧数据和 UI（刷新场景必需；构造函数首次调用时本来就是空的，无害）
    ui->apply_friend_list->clear();
    _unauth_items.clear();

    _min_apply_id = 0;
    _has_more     = true;
    _loading_more = false;
    _fail_count   = 0;

    loadMoreApply();
}


void ApplyFriendPage::slot_auth_rsp(std::shared_ptr<AuthRsp> auth_rsp){
    qDebug() << "ApplyFriendPage::slot_auth_rsp() called";
    auto uid = auth_rsp->_uid;
    auto find_iter = _unauth_items.find(uid);
    if (find_iter == _unauth_items.end()) {
        return;
    }
    // UserMgr::GetInstance()->RemoveOneApply(auth_rsp->_uid); //————> 即使添加好友成功了，也不应该移除这条apply，应该进行保留
    UserMgr::GetInstance()->AddToFriendMap(auth_rsp);

    find_iter->second->ShowAddBtn(false);
}

void ApplyFriendPage::slot_applylist_rsp(const QJsonObject &jsonObj){
    _loading_more = false;
    if(jsonObj["error"].toInt() != ErrorCodes::SUCCESS){
        qDebug() << "服务器并未成功返回applyList";
        // ★ 不把 _has_more 置 false —— 允许用户再滚一次重试
        //   但要防止"一直错、一直发"的请求风暴：连续失败 3 次就停
        if(++_fail_count >= 3){
            _has_more = false;
            qDebug() << "连续 3 次加载失败，已停止自动重试";
        }
        return;
    }
    _fail_count = 0;                     // 成功一次就清零
    const QJsonArray arr = jsonObj["apply_list"].toArray();
    for(const auto& element : arr){
        const QJsonObject jsonObj_2 = element.toObject();
        const qint64 id = static_cast<qint64>(jsonObj_2["id"].toDouble());
        if(_min_apply_id == 0 || _min_apply_id > id){
            _min_apply_id = id;
        }
        std::shared_ptr<AddFriendApply> add_friend_apply = std::make_shared<AddFriendApply>(jsonObj_2["uid"].toInt(), jsonObj_2["name"].toString(),
                                                                                            jsonObj_2["desc"].toString(), jsonObj_2["icon"].toString(), jsonObj_2["nick"].toString(), jsonObj_2["sex"].toInt());
        qDebug() << "uid : " << jsonObj_2["uid"].toInt() << "--------------";
        AddNewApply(add_friend_apply);      //这里会添加apply_list到本组件的子组 件applyfriendlist里，同时tcpMgr里的ID_GET_APPLY_LIST_RSP对应的
                        //handler也会添加到UserMgr的_apply_list里，实现数据的同步。而非applyfriendlist里有，but UserMgr的_apply_list里没有
    }
    _has_more = jsonObj["has_more"].toBool() && !arr.empty(); // 加 && !arr.empty() ——> 防御性编程
}
