#ifndef CUSTOMIZEEDIT_H
#define CUSTOMIZEEDIT_H

#include <QLineEdit>

class CustomizeEdit : public QLineEdit
{
    Q_OBJECT
public:
    CustomizeEdit(QWidget *parent = nullptr);
    void SetMaxLength(std::size_t length);
signals:
    void sig_foucus_out();
    // void sig_clear_search();
protected:
    virtual void focusOutEvent(QFocusEvent *event) override;
    void LimitMaxLen(QString text);
    std::size_t _max_len;
};

#endif // CUSTOMIZEEDIT_H
