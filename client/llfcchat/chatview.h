#ifndef CHATVIEW_H
#define CHATVIEW_H
#include <QWidget>
#include <QScrollArea>

class ChatItemBase;

class ChatView: public QWidget
{
    Q_OBJECT
public:
    ChatView(QWidget* parent = nullptr);
    void appendItem(QWidget* item);     //尾插
    void appendItem(std::shared_ptr<ChatItemBase> item);
    void prependItem(QWidget* item);    //头插
    void insertItem(QWidget *before, QWidget *item); //中间插 ？？？？？？？？？？？
    void removeAllItem();
public slots:
    void onVScrollBarMoved(int min, int max);
protected:
    //virtual bool eventFilter(QObject *watched, QEvent *event);
    virtual bool eventFilter(QObject* watched,QEvent* event)override;
    virtual void paintEvent(QPaintEvent* event)override;
private:
    QScrollArea* _m_pScrollArea;
    bool _be_appending;
};

#endif // CHATVIEW_H
