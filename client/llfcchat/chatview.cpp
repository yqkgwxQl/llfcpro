#include "chatview.h"
#include <QVBoxLayout>
#include <QScrollBar>
#include <QStyleOption>
#include <QPainter>
#include <QEvent>
#include <QTimer>

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
void ChatView::onVScrollBarMoved(int min, int max){
    //这里主要是人为设置500ms来处理,防止突然出现一批滚轮向下滑，造成卡顿
    if(_be_appending){
        QScrollBar* pVScrollBar = _m_pScrollArea->verticalScrollBar();
        pVScrollBar->setSliderPosition(pVScrollBar->maximum());

        QTimer::singleShot(500,[this](){
            _be_appending = false;
        });
    }
}

bool ChatView::eventFilter(QObject* watched,QEvent* event){
    if(watched == _m_pScrollArea && event->type() == QEvent::Enter){
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
