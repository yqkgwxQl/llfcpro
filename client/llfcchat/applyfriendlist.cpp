#include "applyfriendlist.h"
#include <QEvent>
#include <QWheelEvent>
#include <QScrollBar>

ApplyFriendList::ApplyFriendList(QWidget *parent):QListWidget(parent){
    this->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    this->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    // 安装事件过滤器
    this->viewport()->installEventFilter(this);
}

bool ApplyFriendList::eventFilter(QObject *watched, QEvent *event){

    // 检查事件是否是鼠标悬浮进入或离开
    if (watched == this->viewport()) {
        if (event->type() == QEvent::Enter) {
            // 鼠标悬浮，显示滚动条
            this->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
        } else if (event->type() == QEvent::Leave) {
            // 鼠标离开，隐藏滚动条
            this->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        }
    }

     // 检查事件是否是鼠标滚轮事件
    if (watched == this->viewport()) {
        if (event->type() == QEvent::MouseButtonPress) {
            emit sig_show_search(false);
        }
        if(event->type() == QEvent::Wheel){
            QWheelEvent* wheel_event = static_cast<QWheelEvent*>(event);
            int degree = wheel_event->angleDelta().y() / 8;
            int step = degree / 15;
            int steps = step*3;
            this->verticalScrollBar()->setValue(this->verticalScrollBar()->value() - steps);

            auto bar = this->verticalScrollBar();
            if(bar->value() >= bar->maximum())
                emit sig_loading_apply_list();
            return true; // if 没有这一句： QListWidget 看到滚轮事件 → 第 2 次滚动 ❌
        }
    }

    return QListWidget::eventFilter(watched, event);

}