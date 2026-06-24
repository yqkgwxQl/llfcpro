#include "statewidget.h"
#include <QMouseEvent>
#include <QEnterEvent>
#include <QEvent>
#include <QVBoxLayout>
#include <QStyleOption>
#include <QPainter>

StateWidget::StateWidget(QWidget* parent)
    :QWidget(parent),_cur_state(ClickLbState::Normal){
    setCursor(Qt::PointingHandCursor);
    //添加红点
    addRedPoint();
}

void StateWidget::setState(QString normal, QString hover, QString press,QString select, QString select_hover, QString select_press){
    _normal = normal;
    _normal_hover = hover;
    _normal_press = press;
    _selected = select;
    _selected_hover = select_hover;
    _selected_press = select_press;

    _cur_state = ClickLbState::Normal;
    setProperty("state",_normal);
    repolish(this);
    update();
}

void StateWidget::addRedPoint(){
    _red_point = new QLabel();
    _red_point->setObjectName("red_point");
    _red_point->setAlignment(Qt::AlignCenter);
    QVBoxLayout* v_layout = new QVBoxLayout();
    v_layout->setContentsMargins(0,0,0,0);
    v_layout->addWidget(_red_point);
    this->setLayout(v_layout);
    _red_point->setVisible(false);
}

void StateWidget::showRedPoint(){
    _red_point->setVisible(true);
}

void StateWidget::clearState(){
    _cur_state = ClickLbState::Normal;
    setProperty("state",_normal);
    repolish(this);
    update();
}

ClickLbState StateWidget::getCurState(){
    return _cur_state;
}

void StateWidget::setSelected(bool bselected){
    if(bselected){
        _cur_state = ClickLbState::Selected;
        setProperty("state",_selected);
        repolish(this);
        update();
    }else{
        _cur_state = ClickLbState::Normal;
        setProperty("state",_normal);
        repolish(this);
        update();
    }
}


void StateWidget::paintEvent(QPaintEvent* event){
    QStyleOption opt;
    opt.initFrom(this);
    QPainter painter(this);
    style()->drawPrimitive(QStyle::PE_Widget,&opt,&painter,this);
}


void StateWidget::mousePressEvent(QMouseEvent *event){
    if(event->button() == Qt::LeftButton){
        if(getCurState() == ClickLbState::Normal){
            _cur_state = ClickLbState::Selected;
            setProperty("state",_normal_press);
            repolish(this);
            update();
        }else{
            _cur_state = ClickLbState::Normal;
            setProperty("state",_selected_press);
            repolish(this);
            update();
        }
    }
    return QWidget::mousePressEvent(event);
}


void StateWidget::mouseReleaseEvent(QMouseEvent *event){
    if(event->button() == Qt::LeftButton){
        if(getCurState() == ClickLbState::Normal){
            setProperty("state",_normal_hover);
            repolish(this);
            update();
        }else{
            setProperty("state",_selected_hover);
            repolish(this);
            update();
        }
        emit clicked();
    }
    return QWidget::mouseReleaseEvent(event);
}


void StateWidget::enterEvent(QEnterEvent *event){
    if(event->type() == QEnterEvent::Enter){
        if(getCurState() == ClickLbState::Normal){
            setProperty("state",_normal_hover);
            repolish(this);
            update();
        }else{
            setProperty("state",_selected_hover);
            repolish(this);
            update();
        }
    }
    return QWidget::enterEvent(event);
}


void StateWidget::leaveEvent(QEvent *event){
    if(event->type() == QEvent::Leave){
        if(getCurState() == ClickLbState::Normal){
            setProperty("state",_normal);
            repolish(this);
            update();
        }else{
            setProperty("state",_selected);
            repolish(this);
            update();
        }
    }
    return QWidget::leaveEvent(event);
}
