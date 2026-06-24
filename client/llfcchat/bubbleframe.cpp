#include "bubbleframe.h"
#include <QPainter>

const int WIDTH_SANJIAO  = 8;  //三角宽
BubbleFrame::BubbleFrame(ChatRole role, QWidget *parent)
    :QFrame(parent),_role(role),_margin(3){
    _pHLayout = new QHBoxLayout();
    if(_role == ChatRole::Self){
        _pHLayout->setContentsMargins(_margin,_margin,_margin + WIDTH_SANJIAO,_margin);
    }else{
        _pHLayout->setContentsMargins(_margin + WIDTH_SANJIAO,_margin,_margin,_margin);
    }
    this->setLayout(_pHLayout);
}

void BubbleFrame::setWidget(QWidget* w){
    //防御性编程
    if(_pHLayout->count() > 0)
        return ;
    else{
        _pHLayout->addWidget(w);
    }
}

void BubbleFrame::paintEvent(QPaintEvent* event){
    QPainter painter(this);
    painter.setPen(Qt::NoPen);  //去掉笔的边线
    if(_role == ChatRole::Self){
        QColor bk_color(158,234,106);
        painter.setBrush(QBrush(bk_color)); //设置笔刷，笔刷颜色为微信绿
        //画气泡
        QRect bk_rect(0,0,this->width()-(WIDTH_SANJIAO + _margin),this->height());
        painter.drawRoundedRect(bk_rect,5,5);
        /*bk_rect.x()       0（从最左边开始画）
          bk_rect.y()       0（从最顶部开始画）*/
        //画三角  12,20,14
        QPointF points[3] = {
            QPointF(bk_rect.width(),12),
            QPointF(bk_rect.width(),12+WIDTH_SANJIAO +2),
            QPointF(bk_rect.width() + WIDTH_SANJIAO,10+WIDTH_SANJIAO-WIDTH_SANJIAO/2)
        };
        painter.drawPolygon(points,3);
    }else{
        QColor bk_color(Qt::white);
        painter.setBrush(QBrush(bk_color));
        //画气泡
        QRect bk_rect(WIDTH_SANJIAO + _margin,0,this->width()-(WIDTH_SANJIAO + _margin),this->height());
        painter.drawRoundedRect(bk_rect,5,5);
        //画三角  12,20,14
        QPointF points[3] = {
            QPointF(bk_rect.x(),12),
            QPointF(bk_rect.x() ,10+WIDTH_SANJIAO +2),
            QPointF(bk_rect.x() - WIDTH_SANJIAO,10+WIDTH_SANJIAO-WIDTH_SANJIAO/2)
        };
        painter.drawPolygon(points,3);
    }
    return QFrame::paintEvent(event);
}