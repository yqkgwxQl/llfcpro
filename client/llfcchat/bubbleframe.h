#ifndef BUBBLEFRAME_H
#define BUBBLEFRAME_H
#include <QFrame>
#include "global.h"
#include <QHBoxLayout>

class BubbleFrame: public QFrame
{
    Q_OBJECT
public:
    BubbleFrame(ChatRole role, QWidget *parent = nullptr);
    void setWidget(QWidget* w);
protected:
    virtual void paintEvent(QPaintEvent* event)override;
private:
    QHBoxLayout* _pHLayout;
    ChatRole _role;
    std::size_t _margin;
};

#endif // BUBBLEFRAME_H
