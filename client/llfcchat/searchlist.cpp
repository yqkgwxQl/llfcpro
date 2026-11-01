#include "searchlist.h"
#include "tcpmgr.h"
#include <QEvent>
#include <QWheelEvent>
#include <qscrollbar.h>
#include "adduseritem.h"
#include "findsuccessdlg.h"
#include <memory>
#include "loadingdialog.h"
#include "customizeedit.h"
#include <QJsonObject>
#include "findfaildlg.h"
#include "usermgr.h"
#include "findhasfrienddlg.h"

SearchList::SearchList(QWidget *parent)
    :_find_dlg(nullptr),_send_pending(false),_loading_dlg(nullptr),_search_edit(nullptr){
    Q_UNUSED(parent);
    this->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    this->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    //设置过滤器
    this->viewport()->installEventFilter(this);
    //连接点击的信号和槽
    connect(this, &QListWidget::itemClicked, this, &SearchList::slot_item_clicked);
    //添加条目
    addTipItem();
    connect(TcpMgr::GetInstance().get(), &TcpMgr::sig_user_search, this, &SearchList::slot_user_search);
}

// void SearchList::setSearchEdit(CustomizeEdit& search_edit){
//     _search_edit = std::shared_ptr<CustomizeEdit>(&search_edit);
// }

/*这里会不安全吗？我总感觉有点不安全的味道 ————> 不会。 SearchList 和 CustomizeEdit 都是 ChatDialog（.ui 里）的子控件，
 * 生命周期由 ChatDialog 统一管理；_search_edit 只是"观察者"，不负责释放，所以不存在悬垂（只要 ChatDialog 活着两者都活着）。*/
void SearchList::setSearchEdit(CustomizeEdit* search_edit){
    _search_edit = search_edit;
}

bool SearchList::eventFilter(QObject *watched, QEvent *event){
    if(this->viewport() == watched){
        if(event->type() == QEvent::Enter){
            this->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
        }else if(event->type() == QEvent::Leave){
            this->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        }
    }
    //角度/8：度数，度数/15：步数  角度—>度数->步数
    if(this->viewport() == watched && event->type() == QEvent::Wheel){
        //QWheelEvent 不是 QObject 的子类，所以不能用 qobject_cast。qobject_cast 只能用于继承自 QObject 且含有 Q_OBJECT 宏的类之间的转换。
        QWheelEvent* wheel_event = dynamic_cast<QWheelEvent*>(event);
        int degrees = wheel_event->angleDelta().y()/8;
        int steps = degrees/15;
        //设置滚动幅度
        this->verticalScrollBar()->setValue(this->verticalScrollBar()->value() - steps);
        return true; //停止事件传递  ;  if 没有这一句： QListWidget 看到滚轮事件 → 第 2 次滚动 ❌
    }
    return QListWidget::eventFilter(watched,event);
}

void SearchList::slot_user_search(std::shared_ptr<SearchInfo> si){
    waitPending(false);
    //只允许出现一个FindDlg
    if(_find_dlg){
        _find_dlg->hide();
        _find_dlg->deleteLater();
        _find_dlg = nullptr;
    }
    if(si == nullptr){
        // _find_dlg = std::make_shared<FindFailDlg>(this);
        _find_dlg = new FindFailDlg(this);
        _find_dlg->show();
        qDebug() << "create FindFailDlg success";
        return;
    }
    if(si->_uid == UserMgr::GetInstance()->GetUid()){
        return;     //如果是自己的话，什么都不做？？————> 这里后续可以优化，如果输入框里发现了是自己的uid，直接弹出对话框提示搜索用户是自己，而非不对搜索的uid
                                                    //做任何筛选就进行发送
    }

    switch(si->_relation){
    case 0:{
        // _find_dlg = std::make_shared<FindSuccessDlg>(this);     //1.this:交给qt目录树，释放一次  2. make_shared -> 又释放一次
        // std::dynamic_pointer_cast<FindSuccessDlg>(_find_dlg)->setSearchInfo(si);
        FindSuccessDlg* dlg = new FindSuccessDlg(this);
        dlg->setSearchInfo(si);
        _find_dlg = dlg;    //这里是隐式的子类转父类
        qDebug() << "create FindSuccessDlg success";
        _find_dlg->show();
        break;
    }
    case 1:
        // _find_dlg = new FindHasFriendDlg(this);//？？？？？？？？这个组件之后要进行删除
        // qDebug() << "create FindHasFriendDlg success";
        emit sig_jump_chat_item_by_search(si);
        qDebug() << "你所搜索的用户，你已经拥有他/她的好友了，现跳转到他/她的聊天页面";
        break;
    case 2:
        qDebug() << "你所搜索的用户，你已经向她/他提交了申请，目前正在等待对方同意你的申请";//这里应该要在原界面进行提示？？？？如何做呢？
        break;
    case 3:
        // emit sig_jump_applyfriendpage(si); ————> 这里主要是跳转到applyfriendpage页，同时侧边框也要做对应的变化，同时applyfriendpage页如果没有
        //显示对应的搜索用户的申请的话，应该移到applyfriendlist的前侧？？？？？
        qDebug() << "你所搜索的用户，对方已经向你提交了申请，现在请你同意其申请";
        break;
    case 4:
        //这里可能有些复杂，现不进行处理？？？
        break;
    case 5:
        qDebug() << "远端出现数据不一致问题";  //如何进行处理呢？？？？
        break;
    default:
        break;
    }
}

void SearchList::slot_item_clicked(QListWidgetItem *item){
    //获取自定义widget对象
    QWidget* widget = this->itemWidget(item);
    if(!widget){
        qDebug()<< "slot item clicked widget is nullptr";
        return;
    }
    // 对自定义widget进行操作， 将item 转化为基类ListItemBase 从而取出其item_type
    ListItemBase* customItem = qobject_cast<ListItemBase*>(widget);
    if(!customItem){
        qDebug()<< "slot item clicked widget is nullptr";
        return;
    }
    auto item_type = customItem->GetItemType();
    if(item_type == ListItemType::INVALID_ITEM){
        qDebug()<< "slot invalid item clicked ";
        return;
    }
    //这里只管搜索
    if(item_type == ListItemType::ADD_USER_TIP_ITEM){
        if(_send_pending)
            return;
        //防御性编程
        if(!_search_edit){
            return;
        }
        waitPending(true);
        qDebug() << "waitPending success ........." ;
        QString search_uid = _search_edit->text();
        QJsonObject json_obj;
        json_obj["search_uid"] = search_uid;
        QJsonDocument json_doc(json_obj);
        QByteArray json_data = json_doc.toJson(QJsonDocument::Compact);
        emit TcpMgr::GetInstance()->sig_send_data(ID_SEARCH_USER_REQ,json_data);
        return;
    }
    //用户点击其它地方，清除弹出框
    CloseFindDlg();
}

void SearchList::addTipItem(){
    //这个 10px 的分隔线--------------
    auto* item_tmp = new QListWidgetItem();
    item_tmp->setSizeHint(QSize(250,1));
    auto* invalid_item = new QWidget();
    invalid_item->setObjectName("invalid_item");
    //setItemWidget(item, widget) 的作用将一个自定义的 QWidget 设置到 QListWidget里的某个QListWidgetItem中，替换默认的纯文本显示。
    this->addItem(item_tmp);
    this->setItemWidget(item_tmp,invalid_item);
    /*原始 flags:  0000 1111  (可选中 + 启用 + 可勾选 + 可拖拽)
    &
    取反掩码:   1111 0111  (所有位都是1，除了"可选中"位是0)
    =
    结果:       0000 0111  (启用 + 可勾选 + 可拖拽，但不可选中 ✅)*/
    //保留条目的其他所有属性（可用、可勾选、可拖拽），只去掉"可选中"这个属性，让这个条目不能被鼠标点击选中高亮。
    item_tmp->setFlags(item_tmp->flags() & ~Qt::ItemIsSelectable);
    //这个 10px 的分隔线--------------

    auto* add_user_item = new AddUserItem();
    auto* item = new QListWidgetItem();
    item->setSizeHint(add_user_item->sizeHint());
    this->addItem(item);
    this->setItemWidget(item,add_user_item);
}

void SearchList::CloseFindDlg(){
    if(_find_dlg){
        _find_dlg->hide();
        _find_dlg->deleteLater();
        _find_dlg = nullptr;
    }
}

void SearchList::waitPending(bool pending){
    if(pending){
        qDebug() << "SearchList::waitPending(true) ........." ;
        // _loading_dlg = std::make_shared<LoadingDialog>(this);
        if(_loading_dlg){        // ★ 防御：上一次没清干净就先删掉
            _loading_dlg->hide();
            _loading_dlg->deleteLater();
            _loading_dlg = nullptr;
        }
        _loading_dlg = new LoadingDialog(this);
        _loading_dlg->setModal(true);
        _loading_dlg->show();
        _send_pending = pending;
        //这样就只有一个LoadingDlg了
    }else{
        qDebug() << "SearchList::waitPending(false) ........." ;
        if(_loading_dlg){
            _loading_dlg->hide();
            _loading_dlg->deleteLater();    //这里deleterLater()会排队删除一次，but_loading_dlg -> shared_ptr 又会再删一次
            _loading_dlg = nullptr;
        }
        _send_pending = pending;
    }
}
