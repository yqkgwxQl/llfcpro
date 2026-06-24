#include "chatuserlist.h"
#include <QEvent>
#include <QWheelEvent>
#include <QScrollBar>

/*QObject = 对象能力（信号槽、父子树）
QWidget = QObject + 屏幕显示能力（绘制、输入、布局）*/
ChatUserList::ChatUserList(QWidget *parent)
    :QListWidget(parent){
    Q_UNUSED(parent);
    this->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    this->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    //安装事件过滤器
    this->viewport()->installEventFilter(this);
}

ChatUserList::~ChatUserList(){

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
