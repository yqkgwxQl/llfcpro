#ifndef TEXTBUBBLE_H
#define TEXTBUBBLE_H
#include "bubbleframe.h"
#include <QTextEdit>

class TextBubble: public BubbleFrame
{
public:
    TextBubble(ChatRole role, const QString &text, QWidget *parent = nullptr);
protected:
    virtual bool eventFilter(QObject* watched,QEvent* event)override;
private:
    void setPlainText(const QString &text);
    void initStyleSheet();
    void adjustTextHeight();
    QTextEdit* _pTextEdit;
};

#endif // TEXTBUBBLE_H
