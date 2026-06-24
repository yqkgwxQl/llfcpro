#ifndef CLICKEDLABEL_H
#define CLICKEDLABEL_H

#include <QLabel>
#include "global.h"

class ClickedLabel : public QLabel
{
    Q_OBJECT
public:
    ClickedLabel(QWidget *parent=nullptr);
    ~ClickedLabel();
    virtual void mousePressEvent(QMouseEvent *event) override;
    virtual void mouseReleaseEvent(QMouseEvent *event)override;
    virtual void enterEvent(QEnterEvent* event) override;
    virtual void leaveEvent(QEvent* event) override;
    void SetState(QString normal="", QString hover="", QString press="",
                  QString select="", QString select_hover="", QString select_press="");

    ClickLbState GetCurState();
    void ResetNormalState();
    bool SetCurState(ClickLbState state);
signals:
    void clicked(QString, ClickLbState);
private:
    /*ClickLbState定义在global.h中，包含两种状态一个是普通状态，一个是选中状态。而Label中的六种状态就是基于这两种状态嵌套实现的。
    一个Label有六种状态，普通状态，普通的悬浮状态，普通的点击状态，选中状态，选中的悬浮状态，选中的点击状态。
    当Label处于普通状态，被点击后，切换为选中状态，再次点击又切换为普通状态。*/
    QString _normal;
    QString _normal_hover;
    QString _normal_press;

    QString _selected;
    QString _selected_hover;
    QString _selected_press;

    ClickLbState _curstate;
};

#endif // CLICKEDLABEL_H
