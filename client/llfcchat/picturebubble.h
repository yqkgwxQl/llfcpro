#ifndef PICTUREBUBBLE_H
#define PICTUREBUBBLE_H
#include "bubbleframe.h"

class PictureBubble : public BubbleFrame
{
    Q_OBJECT
public:
    PictureBubble(ChatRole role, const QPixmap &picture, QWidget *parent = nullptr);
};
#endif // PICTUREBUBBLE_H
