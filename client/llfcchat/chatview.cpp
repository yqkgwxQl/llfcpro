#include "chatview.h"
#include <QVBoxLayout>
#include <QScrollBar>
#include <QStyleOption>
#include <QPainter>
#include <QEvent>
#include <QTimer>
#include "chatitembase.h"

ChatView::ChatView(QWidget* parent)
    :QWidget(parent),_be_appending(false){
    QVBoxLayout* pMainLayout = new QVBoxLayout();
    this->setLayout(pMainLayout);
    pMainLayout->setContentsMargins(0,0,0,0);

    _m_pScrollArea = new QScrollArea();
    _m_pScrollArea->setObjectName("chat_area");
    pMainLayout->addWidget(_m_pScrollArea);

    QWidget* w = new QWidget(this);
    w->setObjectName("chat_bg");
    w->setAutoFillBackground(true);

    QVBoxLayout* pVLayout_1 = new QVBoxLayout();
    pVLayout_1->addWidget(new QWidget(),100000);
    w->setLayout(pVLayout_1);
    _m_pScrollArea->setWidget(w);

    _m_pScrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    QScrollBar* pVScrollBar = _m_pScrollArea->verticalScrollBar();
    connect(pVScrollBar, &QScrollBar::rangeChanged,this, &ChatView::onVScrollBarMoved);

    //把垂直ScrollBar放到上边 而不是原来的并排
    QHBoxLayout *pHLayout_2 = new QHBoxLayout();
    pHLayout_2->addWidget(pVScrollBar, 0, Qt::AlignRight);
    pHLayout_2->setContentsMargins(0,0,0,0);
    _m_pScrollArea->setLayout(pHLayout_2);
    pVScrollBar->setHidden(true);

    _m_pScrollArea->setWidgetResizable(true);
    _m_pScrollArea->installEventFilter(this);
}
/*pVLayout_1->addWidget(new QWidget(),100000); ————> 100000:拉伸系数(很大)
 不加拉伸（stretch = 0）：

┌─────────────────────┐
│  消息 1              │  ← 气泡：高度 70px
│  消息 2              │  ← 气泡：高度 70px
│                     │  ← 剩余空间没人管
│                     │
│                     │
└─────────────────────┘
所有控件都只占自己需要的尺寸，剩余空间谁都不占

加拉伸（stretch = 100000）：

┌─────────────────────┐
│                     │  ← 空 QWidget  stretch=100000
│                     │    把所有剩余空间全占了
│                     │
│  消息 1              │  ← 气泡
│  消息 2              │  ← 气泡
└─────────────────────┘
有空 widget 撑在上面，消息被挤到底部*/

void ChatView::appendItem(QWidget* item){
    QVBoxLayout *vl = qobject_cast<QVBoxLayout *>(_m_pScrollArea->widget()->layout());
    qDebug() << "vl->count() is " << vl->count();
    vl->insertWidget(vl->count()-1, item);
    // vl->addWidget(item);
    //这里采用未注释的写法
    _be_appending = true;
}

void ChatView::appendItem(std::shared_ptr<ChatItemBase> item){
    QVBoxLayout *vl = qobject_cast<QVBoxLayout *>(_m_pScrollArea->widget()->layout());
    qDebug() << "vl->count() is " << vl->count();
    vl->insertWidget(vl->count()-1, item.get());
    // vl->addWidget(item);
    //这里采用未注释的写法
    _be_appending = true;
}

void ChatView::prependItem(QWidget* item){

}

void ChatView::insertItem(QWidget *before, QWidget *item){

}

/*什么时候pVScrollBar会有rangeChanged信号？
     添加一条消息气泡
        ↓
    气泡被添加到 pVLayout_1 中
        ↓
    pVLayout_1 重新计算高度
        ↓
    w（内容 widget）的高度变化
        ↓
    QScrollArea 检测到内容 widget 高度变化
        ↓
    QScrollArea 内部更新滚动条范围
        ↓
    m_pScrollArea->verticalScrollBar()->setRange(newMin, newMax)
        ↓
    QScrollBar 发现 min/max 变了
        ↓
    发射 rangeChanged(min, max)
        ↓
    你的 onVScrollBarMoved 被调用*/
/*
 * 1.聊天窗口里，新消息是加在底部的。但滚动条不会自己跟着走
所以每次追加消息后必须把滑块推到最底。但你不能在 appendItem 里直接推，因为：
    void ChatView::appendItem(QWidget* item){
        vl->insertWidget(vl->count()-1, item);
        // ← 这一行执行完，布局还没重新计算！
        //    此刻 scrollBar->maximum() 还是"加入之前"的旧值
        //    如果 now 就 setSliderPosition(maximum())，滚到的是旧底部 → 新气泡依然看不见
    2.Qt 的布局是延迟计算的：你把 widget 塞进 layout，Qt 只是记下这笔账，真正算几何、更新滚动条要等这一轮事件循环走完。
        所以"滚到底"这个动作只能等通知 —— 而 rangeChanged 就是那个通知："范围已经刷新，maximum() 现在是准的了，你可以滚了"。
        于是用一个标志把"待办事项"记下来：
            _be_appending = true;   // appendItem 里：记账 —— "这波加完消息，记得贴底"

        if(_be_appending){                                  // 条件满足：确实是"加消息"引起的
            pVScrollBar->setSliderPosition(pVScrollBar->maximum());   // 现在 maximum() 才是对的
            ...
        }
    }
    3.为什么还要等 500ms 才复位
        因为 rangeChanged 可能连着触发很多次：一次插入 10 条消息 → 10 次布局 → 可能 10 次 rangeChanged。每一次都应该贴底（否则前面的都没滚到位）。
        但也不能"永远贴底" —— 如果用户在这一波之后主动往上翻历史，这时你再把他拽到底部就很讨厌。所以：
        "贴底"这个行为只在"刚加过消息"之后的 500ms 内有效，之后放手。
*/
void ChatView::onVScrollBarMoved(int min, int max){
    //这里主要是人为设置500ms来处理,防止突然出现一批滚轮向下滑，造成卡顿
    if(_be_appending){
        QScrollBar* pVScrollBar = _m_pScrollArea->verticalScrollBar();
        pVScrollBar->setSliderPosition(pVScrollBar->maximum());     //滚动条跳转到最底部

        QTimer::singleShot(500,[this](){
            _be_appending = false;
        });
    }
}

bool ChatView::eventFilter(QObject* watched,QEvent* event){
    if(watched == _m_pScrollArea && event->type() == QEvent::Enter){
        //minimum() = 0    maximum() = max(0, 内容高度 - 视口高度)
        // 也就是"最多还能往下滚多少像素"。
        _m_pScrollArea->verticalScrollBar()->setHidden(_m_pScrollArea->verticalScrollBar()->maximum() == 0);
    }

    if(watched == _m_pScrollArea && event->type() == QEvent::Leave){
        _m_pScrollArea->verticalScrollBar()->setHidden(true);
    }
    return QWidget::eventFilter(watched,event);
}

void ChatView::paintEvent(QPaintEvent *event){
    QStyleOption opt;
    opt.initFrom(this);
    QPainter p(this);
    style()->drawPrimitive(QStyle::PE_Widget,&opt,&p,this);
}

void ChatView::removeAllItem(){
    QVBoxLayout* layout = qobject_cast<QVBoxLayout*>(_m_pScrollArea->widget()->layout());
    if(!layout)
        return;

    while(layout->count() > 1){
        QLayoutItem* layout_item = layout->takeAt(0);
        if(!layout_item)
            return;
        QWidget* widget = layout_item->widget();
        if(!widget)
            return;
        delete widget;
        delete layout_item;
    }

    _be_appending = false;
    //没有那行 → 标记一直是 true 悬在那里，等下一次任何无关的 rangeChanged（比如用户拉窗口大小、滚动区布局微调）来临时，就会莫名其妙执行一次"跳到底部"。
}
/*
1.ChatView (QWidget)
 └─ QVBoxLayout (pMainLayout)
     └─ QScrollArea (_m_pScrollArea)
         └─ widget w  ← 滚动内容区（setWidget(w)）
             └─ QVBoxLayout (pVLayout_1)   ★ 就是这个
                 ├─ [0] ChatItemBase   消息1
                 ├─ [1] ChatItemBase   消息2
                 └─ [2] QWidget()      ← "占位撑高"控件（stretch=100000
2.takeAt() 到底做了什么（核心概念）
    这是 Qt 里最容易误解的一处：
    操作                                                  从布局里摘掉                   销毁控件
    layout->removeWidget(w)                                 ✅                           ❌（widget 还活着，只是不再被布局管理）
    layout->takeAt(i)                                       ✅                           ❌（返回 QLayoutItem*，所有权交给你）
    layout->takeAt(i) + delete item                         ✅                           ❌（只销毁那个"包装对象"）
    layout->takeAt(i) + delete item->widget() + delete item	✅                           ✅

    为什么会这样？因为 addWidget 时 Qt 会创建两个独立堆对象：
        QWidgetItem (QLayoutItem 的子类)   ← 包装器，只存一个 QWidget* 指针
                └─ 指向 →  QWidget（气泡行）  ← 真正的控件，parent 是容器 w
    takeAt() 只是把 QWidgetItem 从布局的链表里摘出来；
    它不删 QWidgetItem（否则你没法拿返回值），也不删 QWidget；
    所以两者都得你手动 delete。
3.
    [消息1, 消息2, 消息3, 占位]   count=4
    takeAt(0) → 删掉消息1，剩下的前移
    [消息2, 消息3, 占位]         count=3
    takeAt(0) → 删掉消息2
    [消息3, 占位]                count=2
    takeAt(0) → 删掉消息3
    [占位]                       count=1 → 循环结束
*/
