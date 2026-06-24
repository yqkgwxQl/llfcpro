#include "clickedbtn.h"
#include "global.h"
#include "QMouseEvent"
#include "chatdialog.h"

ClickedBtn::ClickedBtn(QWidget *parent)
    :QPushButton(parent){
    setCursor(Qt::PointingHandCursor);
}

ClickedBtn::~ClickedBtn(){

}

void ClickedBtn::SetState(QString normal,QString hover,QString press){
    _normal = normal;
    _hover = hover;
    _press = press;
    setProperty("state",_normal);
    repolish(this);
    update();
}

void ClickedBtn::enterEvent(QEnterEvent *event){
    setProperty("state",_hover);
    repolish(this);
    update();
    QPushButton::enterEvent(event);
}

void ClickedBtn::leaveEvent(QEvent *event){
    setProperty("state",_normal);
    repolish(this);
    update();
    QPushButton::leaveEvent(event);
}

void ClickedBtn::mousePressEvent(QMouseEvent *event){
    if(event->button() == Qt::LeftButton){
        setProperty("state",_press);
        repolish(this);
        update();
    }
    QPushButton::mousePressEvent(event);
}

void ClickedBtn::mouseReleaseEvent(QMouseEvent *event){
    if(event->button() == Qt::LeftButton){
        setProperty("state",_hover);
        repolish(this);
        update();
    }
    QPushButton::mouseReleaseEvent(event);
}
