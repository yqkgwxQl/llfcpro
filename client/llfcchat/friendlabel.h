#ifndef FRIENDLABEL_H
#define FRIENDLABEL_H

#include <QFrame>

namespace Ui {
class FriendLabel;
}

class FriendLabel : public QFrame
{
    Q_OBJECT

public:
    explicit FriendLabel(QWidget *parent = nullptr);
    ~FriendLabel();
    void SetText(const QString& text);
    int Width();
    int Height();
    QString Text();
private:
    Ui::FriendLabel *ui;
    QString _text;
    int _width;
    int _height;    //_width 和 _height 目前不知道要干什么？？？？？？？？？？？？？？
public slots:
    void slot_close();
signals:
    void sig_close(QString);
};

#endif // FRIENDLABEL_H
