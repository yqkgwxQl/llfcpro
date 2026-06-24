#ifndef STATEWIDGET_H
#define STATEWIDGET_H
#include "global.h"
#include <QWidget>
#include <QLabel>

class StateWidget: public QWidget
{
    Q_OBJECT
public:
    StateWidget(QWidget* parent = nullptr);
    void setState(QString normal="", QString hover="", QString press="",
                  QString select="", QString select_hover="", QString select_press="");
    void addRedPoint();
    void showRedPoint();
    void clearState();
    ClickLbState getCurState();
    void setSelected(bool bselected);
signals:
    void clicked();
protected:
    virtual void paintEvent(QPaintEvent* event)override;
    virtual void mousePressEvent(QMouseEvent *event)override;
    virtual void mouseReleaseEvent(QMouseEvent *event)override;
    virtual void enterEvent(QEnterEvent *event)override;
    virtual void leaveEvent(QEvent *event)override;
private:
    QString _normal;
    QString _normal_hover;
    QString _normal_press;
    QString _selected;
    QString _selected_hover;
    QString _selected_press;

    ClickLbState _cur_state;
    QLabel* _red_point;
};

#endif // STATEWIDGET_H
