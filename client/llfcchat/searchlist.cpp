#include "searchlist.h"
#include "tcpmgr.h"
#include <QEvent>
#include <QWheelEvent>
#include <qscrollbar.h>
#include "adduseritem.h"
#include "findsuccessdlg.h"
#include <memory>

SearchList::SearchList(QWidget *parent){
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

bool SearchList::eventFilter(QObject *watched, QEvent *event){
    if(this->viewport() == watched){
        if(event->type() == QEvent::Enter){
            this->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
        }else{
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

void SearchList::slot_user_search(std::shared_ptr<SearchInfo>){

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
    if(item_type == ListItemType::ADD_USER_TIP_ITEM){
        _find_dlg = std::make_shared<FindSuccessDlg>(this);
        //SearchInfo(int uid, QString name, QString nick, QString desc, int sex, QString icon);
        auto si = std::make_shared<SearchInfo>(0,"llfc","llfc","hello , my friend!",1,"");
        std::dynamic_pointer_cast<FindSuccessDlg>(_find_dlg)->setSearchInfo(si);
        _find_dlg->show();
        return;
    }
    //清楚弹出框??????????????????????????????
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
        _find_dlg = nullptr; //引用计数 - 1
    }
}

