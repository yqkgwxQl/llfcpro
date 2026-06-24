
#include "timerbtn.h"
#include <QTimer>
#include <QMouseEvent>

TimerBtn::TimerBtn(QWidget *parent):QPushButton(parent),_counter(10){
    _timer = new QTimer(this);
    connect(_timer,&QTimer::timeout,[this](){
        _counter--;
        if(_counter <= 0){
            _timer->stop();
            _counter = 10;
            this->setText(tr("获取"));
            this->setEnabled(true);
        }else{
            this->setText(QString::number(_counter));
        }
    });
}

TimerBtn::~TimerBtn(){
    _timer->stop();
}

void TimerBtn::mouseReleaseEvent(QMouseEvent *e){
    QPushButton::mouseReleaseEvent(e);  // ← 最先调用基类，发射 clicked()
    if(e->button() == Qt::LeftButton){
        qDebug()<< "MyButton was released!";
        this->setEnabled(false);         // ← 基类处理完毕后再禁用
        this->setText(QString::number(_counter));
        _timer->start(1000);
    }
    // QPushButton::mouseReleaseEvent(e);  // ← 最先调用基类，发射 clicked()
}
