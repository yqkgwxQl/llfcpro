#include "chatuserlist.h"
#include <QEvent>
#include <QWheelEvent>
#include <QScrollBar>
#include "userdata.h"
#include "usermgr.h"

/*QObject = 对象能力（信号槽、父子树）
QWidget = QObject + 屏幕显示能力（绘制、输入、布局）*/
ChatUserList::ChatUserList(QWidget *parent)
    :QListWidget(parent){
    Q_UNUSED(parent);

    connect(this, &QListWidget::itemClicked, this, &ChatUserList::slot_item_clicked);
    this->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    this->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    //安装事件过滤器
    this->viewport()->installEventFilter(this);
}

ChatUserList::~ChatUserList(){

}

//既然，我都可以点击此条目了，我就认为UserMgr的_friend_map里一定有对应的friend_info信息
void ChatUserList::slot_item_clicked(QListWidgetItem* item){
    int chat_other_uid = item->data(Qt::UserRole).toInt();
    auto friend_info = UserMgr::GetInstance()->GetFriendInfoByUid(chat_other_uid);
    if(!friend_info)
        return;
    emit sig_jump_chat_item_by_click(friend_info);
}

/*event->type() == QEvent::Enter  你怎么知道它是鼠标进入，而不是别的什么事件进入？
 *  Qt 源码中 QEvent 的定义（简化）
class QEvent {
public:
    enum Type {
        None = 0,
        Timer = 1,
        MouseButtonPress = 2,
        MouseButtonRelease = 3,
        MouseMove = 5,
        KeyPress = 6,
        KeyRelease = 7,
        Enter = 10,         // ← 鼠标进入控件
        Leave = 11,         // ← 鼠标离开控件
        FocusIn = 8,        // ← 获得焦点
        FocusOut = 9,       // ← 失去焦点
        Resize = 14,        // ← 控件大小改变
        Paint = 12,         // ← 绘制
        ...
    };
};*/


/*      1.对象A->installEventFilter(过滤器B); 之后，每一个发给 A 的事件，都会先经过 B 的 eventFilter()，然后才轮到 A 自己处理。
 *          本文件中安装过滤器的代码如下：
 *              this->viewport()->installEventFilter(this);
 *          这里this->viewport() : watched， 被监视者，自己的子控件。 this: 过滤器，自己
 *              你是否会有疑问：自己过滤自己的子控件? ———— 因为这是两个不同的对象
*                                               this             // ChatUserList，父控件（因为它重写了 eventFilter()，所以有资格进行过滤）
                                                this->viewport() // 一个独立的 QWidget，子控件    ≠ this
                所以准确说法是「父控件监视自己的子控件」，不是"自己监视自己"
                所以说bool ChatUserList::eventFilter(QObject* watched,QEvent* event)实际上就是在重载我自己的eventFilter，使得我有资格作为过滤器
                    因此，很多QObject对象都可以被我监视       if(watched == this->viewport()) ————> why会有呢？因为一个过滤器可以装在多个对象上
 *
        2.watched	被监视者 —— 这个事件本来是发给谁的	*/
/*
 * 1.QListWidget的继承链：QObject → QWidget → QFrame → QAbstractScrollArea → QAbstractItemView → QListView → **QListWidget**
 *                                  ↑
                        注意这里：QAbstractScrollArea

    2.内部的QAbstractScrollArea
        QAbstractScrollArea（可滚动区域）不是一个"单一控件"，它内部由几块拼起来：
        ┌─ QListWidget ─────────────────────────────┐
        │ ┌─ viewport() ────────────────┐ ┌───────┐ │
        │ │                             │ │  ▲    │ │  ← verticalScrollBar()
        │ │   真正显示列表条目的区域      │ │  │    │ │
        │ │   （一个独立的 QWidget，     │ │  ▼    │ │
        │ │     objectName 通常是        │ └───────┘ │
        │ │     "qt_scrollarea_viewport"）│           │
        │ │                             │           │
        │ └─────────────────────────────┘           │
        │ ◄════ horizontalScrollBar() ═════════════►│
        └───────────────────────────────────────────┘
    3.关键点：
        1_  viewport() 是一个独立子控件，占据了除滚动条以外的全部面积；
        2_  所以 鼠标事件（Enter / Leave / Wheel / 点击）都发给 viewport()，而不是发给 QListWidget 本身；
                所以必须装在 viewport 上
                this->viewport()->installEventFilter(this);   // ✅ 能收到滚轮
                // this->installEventFilter(this);            // ❌ 收不到 viewport 上的滚轮
        3_  this->viewport() 返回的是指向那个子控件的指针。

      注意：针对2_ 提出问题：QListWidget不是继承QAbstractScrollArea 吗？viewport() 是QAbstractScrollArea 里的一个子控件，
        那鼠标事件（Enter / Leave / Wheel / 点击）都发给 viewport()，不就相当于发给 QListWidget 本身了吗？

      对象树：
        QListWidget (QListWidget)                 ← 你写的类
         ├── viewport()          (QWidget)        ← new QWidget(q)
         ├── verticalScrollBar() (QScrollBar)
         ├── horizontalScrollBar()(QScrollBar)
         └── 两个 corner widget
            QListWidget 拥有一个叫 viewport() 的独立 QWidget 子对象（源码 new QWidget(q)，运行时类型是 QWidget）。
            事件的第一接收者是 viewport 这个对象，所以 installEventFilter 必须装它；
            但 QListWidget 会通过 viewportEvent() 把鼠标/滚轮/拖放事件“捞回来”自己处理

 */
//这个eventFilter(QObject* watched,QEvent* event)可能未来会被其它QObject复用，用watched == this->viewport()来区分是否是this->viewport()
bool ChatUserList::eventFilter(QObject* watched,QEvent* event){
    // 检查事件是否是鼠标悬浮进入或离开
    if(watched == this->viewport()){
        if(event->type() == QEvent::Enter){
            this->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
        }else if(event->type() == QEvent::Leave){
            this->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        }
    }
    // 检查事件是否是鼠标滚轮事件
    if(watched == this->viewport() && event->type() == QEvent::Wheel){
        QWheelEvent* wheel_event = static_cast<QWheelEvent*>(event);
        int degrees = wheel_event->angleDelta().y() / 8;     //angleData().y() 滚轮滚动的“角度”，除以8得到"度数"，再除以15得到"步数"
        int steps = degrees/15;
        //设置滚动幅度
        this->verticalScrollBar()->setValue(this->verticalScrollBar()->value() - steps);
        /*  这里setValue(this->verticalScrollBar()->value() - steps); 为什么是减号？
            --滚动条 value：
                value = 0      ← 顶部
                value = max    ← 底部
                ↑ 鼠标向上拨滚轮  →  value 减小
                ↓ 鼠标向下拨滚轮  →  value 增大
            --用户向上拨滚轮（想往上翻）：
                angleDelta().y() = 120   →  numSteps = 1
                setValue(value - 1)      →  value 减小 → 内容往下移 → 看到上面内容 ✅

            --用户向下拨滚轮（想往下翻）：
            angleDelta().y() = -120  →  numSteps = -1
            setValue(value - (-1))   →  setValue(value + 1) → value 增大 → 内容往上移 → 看到下面内容 ✅*/
        //检查是否滚动到底部
        QScrollBar* scroll_bar = this->verticalScrollBar();
        int max_scroll_bar = scroll_bar->maximum();
        int cur_scroll_bar = scroll_bar->value();
        if(cur_scroll_bar >= max_scroll_bar){
            // 滚动到底部，加载新的联系人
            qDebug()<<"load more chat user";
            //发送信号通知聊天界面加载更多聊天内容
            emit sig_loading_chat_user();
        }
        return true;    // if 没有这一句： QListWidget 看到滚轮事件 → 第 2 次滚动 ❌
    }
    return QListWidget::eventFilter(watched,event);
}
