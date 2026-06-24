
#ifndef TIMERBTN_H
#define TIMERBTN_H
#include <QPushButton>

class TimerBtn: public QPushButton
{
public:
    TimerBtn(QWidget *parent = nullptr);
    ~TimerBtn();
    // 重写mouseReleaseEvent
    /*virtual	  基类	      声明该函数可被派生类重写，启用多态
      override	  派生类	   显式声明这是对基类虚函数的重写，让编译器帮你校验签名一致*/
    virtual void mouseReleaseEvent(QMouseEvent *e) override;
private:
    QTimer* _timer;
    int _counter;
};

#endif // TIMERBTN_H
