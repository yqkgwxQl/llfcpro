#include "friendlabel.h"
#include "ui_friendlabel.h"

FriendLabel::FriendLabel(QWidget *parent)
    : QFrame(parent)
    , ui(new Ui::FriendLabel)
{
    ui->setupUi(this);
    ui->close_lb->SetState("normal","hover","selected","selected_normal","selected_hover","selected_press");
    connect(ui->close_lb,&ClickedLabel::clicked,this,&FriendLabel::slot_close);
}

FriendLabel::~FriendLabel()
{
    delete ui;
}

void FriendLabel::SetText(const QString& text)
{
    _text = text;
    QFontMetrics font_metrics(ui->tip_lb->font());
    auto text_height = font_metrics.height();
    auto text_width = font_metrics.horizontalAdvance(ui->tip_lb->text());
    qDebug()<< " ui->tip_lb.width() is " << ui->tip_lb->width();
    qDebug()<< " ui->close_lb->width() is " << ui->close_lb->width();
    qDebug()<< " textWidth is " << text_width;
    this->setFixedWidth(ui->tip_lb->width()+ui->close_lb->width()+5);
    this->setFixedHeight(text_height+2);
    qDebug()<< "  this->setFixedHeight " << this->height();
    ui->tip_lb->setText(text);
    _width = this->width();
    _height = this->height();
}

void FriendLabel::slot_close()
{
    emit sig_close(_text);
}

int FriendLabel::Width()
{
    return _width;
}

int FriendLabel::Height()
{
    return _height;
}

QString FriendLabel::Text()
{
    return _text;
}