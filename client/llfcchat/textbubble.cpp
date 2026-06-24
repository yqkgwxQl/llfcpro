#include "textbubble.h"
#include <QEvent>
#include <QTextDocument>
#include <QFontMetricsF>
#include <QTextBlock>

TextBubble::TextBubble(ChatRole role, const QString &text, QWidget *parent)
    :BubbleFrame(role,parent){
    _pTextEdit = new QTextEdit();
    _pTextEdit->setReadOnly(true);
    _pTextEdit->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    _pTextEdit->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    QFont font("Microsoft YaHei");
    font.setPointSize(12);
    _pTextEdit->setFont(font);

    _pTextEdit->installEventFilter(this);
    setWidget(_pTextEdit);
    setPlainText(text);
    initStyleSheet();
}

//我们拉伸的时候要调整气泡的高度,这里重写事件过滤器
bool TextBubble::eventFilter(QObject* watched,QEvent* event){
    if(watched == _pTextEdit && event->type() == QEvent::Paint){
        adjustTextHeight();
    }
    return BubbleFrame::eventFilter(watched,event);
}

void TextBubble::setPlainText(const QString &text){
    //每个textEdit也有对应的内边距
    _pTextEdit->setPlainText(text);
    qreal doc_margin = _pTextEdit->document()->documentMargin();
    std::size_t margin_left = this->layout()->contentsMargins().left();
    std::size_t margin_right = this->layout()->contentsMargins().right();
    QFontMetricsF fm(_pTextEdit->font());
    QTextDocument* doc = _pTextEdit->document();
    int max_length = 0;
    /*"Hi"        → fm.width("Hi")        ≈ 20px
      "你好世界"   → fm.width("你好世界")   ≈ 80px
      "Hello World" → fm.width("Hello World") ≈ 90px*/
    for(QTextBlock it = doc->begin();it != doc->end();it = it.next()){
        //qobject_cast只能用于QObject的派生类，不能用于基本类型
        int txt_width = int(fm.horizontalAdvance(it.text()));
        max_length = max_length < txt_width ? txt_width : max_length;
    }
    //设置这个气泡的最大宽度 只需要设置一次
    setMaximumWidth(max_length + doc_margin*2 + (margin_left + margin_right) + 1);
    // setMaximumWidth(max_length + doc_margin*2 + (margin_left + margin_right));
    qDebug() << "max_length:" << max_length
             << "doc_margin*2:" << doc_margin*2
             << "margins:" << margin_left << margin_right
             << "total:" << max_length + doc_margin*2 + margin_left + margin_right;
    // 在 setWidget 之后，打印 BubbleFrame 的实际宽度
    qDebug() << "BubbleFrame actual width:" << this->width();       //实际布局所给
    qDebug() << "BubbleFrame max width:" << this->maximumWidth();
    //setMaximumWidth 的计算刚好卡在边界上。 horizontalAdvance 返回 85（截断后的整数），但实际文字渲染需要 85.3px。文字区域 = 85px < 实际需要 85.3px → QTextEdit 在空格处换行。
}
/*那这个setPlainText(text);是不是没用？
 * 短文本 "Hi"：
    max_length ≈ 20px
    setMaximumWidth ≈ 42px
    ↓
    布局分配 500px → 气泡卡在 42px ✅ 起作用了
    防止气泡被拉得特别宽


中等文本 "hello world"：
    max_length ≈ 85px
    setMaximumWidth ≈ 108px
    ↓
    布局分配 500px → 气泡卡在 108px ✅ 起作用了
    但 108px 时文字区域 86px 刚好够，不再换行


长文本（1330px 最长行）：
    max_length ≈ 1330px
    setMaximumWidth ≈ 1353px
    ↓
    布局分配 640px → 气泡 640px < 1353px ❌ 上限没被触达
    实际宽度由布局决定，setMaximumWidth 没起作用
*/

void TextBubble::initStyleSheet(){
    _pTextEdit->setStyleSheet("QTextEdit{background:transparent;border:none}");
}

void TextBubble::adjustTextHeight(){
    qreal doc_margin = _pTextEdit->document()->documentMargin();
    QTextDocument *doc = _pTextEdit->document();
    qreal text_height = 0;
    //把每一段的高度相加=文本高
    for(QTextBlock it = doc->begin(); it != doc->end(); it = it.next()){
        QTextLayout* p_layout = it.layout();
        QRectF text_rect = p_layout->boundingRect();
        text_height += text_rect.height();
    }
    std::size_t top_margin = this->layout()->contentsMargins().top();
    std::size_t bottom_margin = this->layout()->contentsMargins().bottom();
    //设置这个气泡需要的高度文本高+文本边距+TextEdit边框到气泡边框的距离
    setFixedHeight(text_height + doc_margin*2 + top_margin + bottom_margin);
}