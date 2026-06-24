#include "clickedlabel.h"
#include "QMouseEvent"

ClickedLabel::ClickedLabel(QWidget *parent):QLabel(parent),_curstate(ClickLbState::Normal) {

}

ClickedLabel::~ClickedLabel(){

}

void ClickedLabel::mousePressEvent(QMouseEvent *event){
    if(event->button() == Qt::LeftButton){
        if(_curstate == ClickLbState::Normal){
            qDebug()<<"clicked , change to selected hover: "<< _selected_hover;
            _curstate = ClickLbState::Selected;
            setProperty("state",_selected_press);//_selected_press
            repolish(this);
            update();
        }else{
            qDebug()<<"clicked , change to normal hover: "<< _normal_hover;
            _curstate = ClickLbState::Normal;
            setProperty("state",_normal_press);
            repolish(this);
            update();
        }
        // emit clicked();
    }
    QLabel::mousePressEvent(event);
}

void ClickedLabel::mouseReleaseEvent(QMouseEvent *event){
    if (event->button() == Qt::LeftButton) {
        if(_curstate == ClickLbState::Normal){
            // qDebug()<<"ReleaseEvent , change to normal hover: "<< _normal_hover;
            setProperty("state",_normal_hover);
            repolish(this);
            update();

        }else{
            //  qDebug()<<"ReleaseEvent , change to select hover: "<< _selected_hover;
            setProperty("state",_selected_hover);
            repolish(this);
            update();
        }
        emit clicked(this->text(),_curstate);
        return;
    }
    // 调用基类的mousePressEvent以保证正常的事件处理
    QLabel::mouseReleaseEvent(event);
}

void ClickedLabel::enterEvent(QEnterEvent *event){
    if(_curstate == ClickLbState::Normal){
        qDebug()<<"enter , change to normal hover: "<< _normal_hover;
        setProperty("state",_normal_hover);
        repolish(this);
        update();
    }else{
        qDebug()<<"enter , change to selected hover: "<< _selected_hover;
        setProperty("state",_selected_hover);
        repolish(this);
        update();
    }
}

void ClickedLabel::leaveEvent(QEvent *event){
    if(_curstate == ClickLbState::Normal){
        qDebug()<<"leave , change to normal : "<< _normal;
        setProperty("state",_normal);
        repolish(this);
        update();
    }else{
        qDebug()<<"leave , change to select hover: "<< _selected;
        setProperty("state",_selected);
        repolish(this);
        update();
    }
}

void ClickedLabel::SetState(QString normal, QString hover, QString press, QString select, QString select_hover, QString select_press){
    _normal = normal;
    _normal_hover = hover;
    _normal_press = press;

    _selected = select;
    _selected_hover = select_hover;
    _selected_press = select_press;

    //初始时设置setProperty("state",normal);  ————> 以后每次点击，离开，进入，都会再 setProperty();
    setProperty("state",normal);
    repolish(this);
}

ClickLbState ClickedLabel::GetCurState(){
    return _curstate;
}

void ClickedLabel::ResetNormalState(){
    _curstate = ClickLbState::Normal;
    setProperty("state", _normal);
    repolish(this);
}

bool ClickedLabel::SetCurState(ClickLbState state)
{
    _curstate = state;
    if (_curstate == ClickLbState::Normal) {
        setProperty("state", _normal);
        repolish(this);
        update();
    }
    else if (_curstate == ClickLbState::Selected) {
        setProperty("state", _selected);
        repolish(this);
        update();
    }

    return true;
}
