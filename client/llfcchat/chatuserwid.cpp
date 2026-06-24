#include "chatuserwid.h"
#include "ui_chatuserwid.h"
#include <QPixmap>

ChatUserWid::ChatUserWid(QWidget *parent)
    : ListItemBase(parent), ui(new Ui::ChatUserWid){
    ui->setupUi(this);
}

ChatUserWid::~ChatUserWid(){
    delete ui;
}


QSize ChatUserWid::sizeHint()const{
    return QSize(250,70); //返回自定义尺寸
}

void ChatUserWid::SetInfo(QString name, QString head, QString msg){
    _name = name;
    _head = head;
    _msg = msg;
    // 加载图片
    QPixmap pixmap(_head);
    ui->icon_lb->setPixmap(
        pixmap.scaled(                  //①先缩放
            ui->icon_lb->size(),       //缩放到Label的大小
            Qt::KeepAspectRatio,     //保持宽高比
            Qt::SmoothTransformation //平滑缩放
        ));
    ui->icon_lb->setScaledContents(true); // 告诉 Label："当 Label 的大小变化时，自动调整图片填满"

    //这两段代码的作用是：确保无论用户的名字多长、发的消息多长，都不会破坏列表项的布局，超出部分会自动变成优雅的省略号 (...)。
    QFontMetrics font_metrics_name(ui->user_name_lb->font());   //fontMetrics 能够精确计算出特定字体下，任意字符串在屏幕上占据的像素宽度。
    /*Qt::ElideLeft: ...放在左边 (如 "...orld")  ui->user_name_lb->width() (最大宽度限制)：这是最关键的一个参数。它告诉函数：“这个标签只有这么宽（像素），
      请帮我算一下怎么截断”。 假设标签宽度只能容纳 "张三丰" 三个字加上省略号的宽度：
                        如果 _name 是 "李四" -> 结果："李四" (不截断)
                        如果 _name 是 "欧阳锋大侠" -> 结果："欧阳..." (自动截断并补全省略号)*/
    QString name_text = font_metrics_name.elidedText(_name,Qt::ElideRight,ui->user_name_lb->width());
    QFontMetrics font_metrics_msg(ui->user_chat_lb->font());
    QString msg_text = font_metrics_msg.elidedText(_msg,Qt::ElideRight,ui->user_chat_lb->width());

    ui->user_name_lb->setText(name_text);
    ui->user_chat_lb->setText(msg_text);
    // ui->user_name_lb->setText(_name);
    // ui->user_chat_lb->setText(_msg);
}
