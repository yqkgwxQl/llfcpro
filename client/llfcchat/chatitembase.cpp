#include "chatitembase.h"
#include <QFont>
#include <QSpacerItem>
#include <QGridLayout>

/*我有个问题，这里没有使用目录树机制，难道不会内存泄露吗？
-- 方式 A：传 parent 参数（显式）
_pNameLabel = new QLabel(this);

-- 方式 B：不传 parent，但后面 addWidget（隐式）
_pNameLabel = new QLabel();                    // 此时 parent = nullptr
pGridLayout->addWidget(_pNameLabel, ...);      // addWidget 内部会 setParent
this->setLayout(pGridLayout) 把布局设置到 ChatItemBase 上，此时 ChatItemBase 接管了 pGridLayout 的所有权。
当 ChatItemBase 销毁时，pGridLayout 也会被销毁*/
ChatItemBase::ChatItemBase(ChatRole role,QWidget* parent)
    :_role(role),QWidget(parent){
    _pNameLabel = new QLabel();
    _pNameLabel->setObjectName("chat_user_name");
    QFont name_font("Microsoft YaHei");
    name_font.setPointSize(9);  //字号设为 9 磅
    _pNameLabel->setFont(name_font);
    _pNameLabel->setFixedHeight(20);

    _pIconLabel = new QLabel();
    _pIconLabel->setScaledContents(true);
    _pIconLabel->setFixedSize(42,42);

    _pBubble = new QWidget();
                                        //40:最小长度，20：最小宽度，QSizePolicy::Expanding ——> 水平策略，QSizePolicy::Minimum ——> 垂直策略
    QSpacerItem* pSpacer = new QSpacerItem(40,20,QSizePolicy::Expanding,QSizePolicy::Minimum);
    QGridLayout* pGridLayout = new QGridLayout();
    /*Qt setContentsMargins	(left, top, right, bottom)	左 → 上 → 右 → 下（顺时针）
     CSS margin / padding	(top, right, bottom, left)	上 → 右 → 下 → 左（顺时针）*/
    pGridLayout->setContentsMargins(3,3,3,3);
    pGridLayout->setHorizontalSpacing(3);
    pGridLayout->setVerticalSpacing(3);

    if(_role == ChatRole::Self){
        _pNameLabel->setContentsMargins(0,0,8,0);
        _pNameLabel->setAlignment(Qt::AlignRight);
        pGridLayout->addWidget(_pNameLabel,0,1,1,1);
        pGridLayout->addWidget(_pIconLabel,0,2,2,1,Qt::AlignTop);
        pGridLayout->addWidget(_pBubble,1,1,1,1);
        pGridLayout->addItem(pSpacer,1,0,1,1);
        pGridLayout->setColumnStretch(0,2);
        pGridLayout->setColumnStretch(1,3);
    }else{
        _pNameLabel->setContentsMargins(8,0,0,0);
        _pNameLabel->setAlignment(Qt::AlignLeft);
        pGridLayout->addWidget(_pNameLabel,0,1,1,1);
        pGridLayout->addWidget(_pIconLabel,0,0,2,1,Qt::AlignTop);
        pGridLayout->addWidget(_pBubble,1,1,1,1);
        pGridLayout->addItem(pSpacer,1,2,1,1);
        pGridLayout->setColumnStretch(2,2);
        pGridLayout->setColumnStretch(1,3);
    }

    this->setLayout(pGridLayout);
}

void ChatItemBase::setUserName(const QString& name){
    _pNameLabel->setText(name);
}

void ChatItemBase::setUserIcon(const QPixmap& icon){
    _pIconLabel->setPixmap(icon);
}

void ChatItemBase::setWidget(QWidget *w){
    QGridLayout* grid_layout = qobject_cast<QGridLayout*>(this->layout());
    grid_layout->replaceWidget(_pBubble,w);  //把网格布局中 _pBubble 的位置让给 w，然后 _pBubble 被踢出布局。
    delete _pBubble;
    _pBubble = w;
}