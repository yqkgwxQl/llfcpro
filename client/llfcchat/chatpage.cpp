#include "chatpage.h"
#include "ui_chatpage.h"
#include <QStyleOption>
#include <QPaintEvent>
#include <QPainter>
#include "chatitembase.h"
#include "textbubble.h"
#include "picturebubble.h"
#include "messagetextedit.h"

ChatPage::ChatPage(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::ChatPage)
{
    ui->setupUi(this);
    //设置按钮样式
    ui->receive_btn->SetState("normal","hover","press");
    ui->send_btn->SetState("normal","hover","press");
    //设置图标样式
    ui->emo_lb->SetState("normal","hover","press","normal","hover","press");
    ui->file_lb->SetState("normal","hover","press","normal","hover","press");
    // ChatPage 构造函数里加一行 (但是不需要，qt6中即是不重写QWidget的paintEvent，对于一些qss属性也是生效的）
    // ui->tool_wid->setStyleSheet("background:#ffffff; border-bottom:0.5px solid #ececec;");

    // 连接 Enter 发送信号
    connect(ui->chat_edit, &MessageTextEdit::send, this, &ChatPage::on_send_btn_clicked);
}

ChatPage::~ChatPage()
{
    delete ui;
}
/*ChatPage 的 paintEvent 对谁起作用？      只对 ChatPage 自己身上的 QSS 起作用
 对子控件有影响吗？                          没有，每个子控件自己画自己的
该不该重写？                                  该，这是标准写法，保证任何 Qt 版本、任何 QSS 属性都正常生效*/
//if不重写paintEvent的话 QWidget不会使你的ChatPage自身的qss生效 (子空间if有QWidget也需要重写才能对自己的qss生效)，
        //反之继承自QWidget的QLabel和QPushButton的paintEvent会自动重写，能够使你的qss生效
/*eg:  在qss中添加          ————>     这个例子可以看出重写paintEvent的必要性
ChatPage {
    background-color: rgb(200, 200, 200);
}*/
void ChatPage::paintEvent(QPaintEvent *event){
    QStyleOption opt;            // ① 创建一个样式选项对象
    opt.initFrom(this);              // ② 用当前控件的信息初始化它（大小、状态等）

    QPainter p(this);            // ③ 创建一个画家，画在当前控件上

    //它告诉 Qt 的样式系统："请按照当前主题和 QSS 规则，把这个控件画出来。"
    style()->drawPrimitive(      // ④ 调用当前风格的绘制方法
        QStyle::PE_Widget,       //    绘制"Widget"图元
        &opt,                    //    传入样式选项
        &p,                      //    传入画家
        this                     //    传入控件本身
        );
}
void ChatPage::on_send_btn_clicked(){
    auto pTextEdit = ui->chat_edit;
    ChatRole role = ChatRole::Self;
    QString userName = QStringLiteral("恋恋风辰");
    QString userIcon = ":/res/head_1.jpg";

    const QVector<MsgInfo>& msgList = pTextEdit->getMsgList();
    for(int i=0; i<msgList.size(); ++i){
        QString type = msgList[i].msgFlag;
        ChatItemBase *pChatItem = new ChatItemBase(role);
        pChatItem->setUserName(userName);
        pChatItem->setUserIcon(QPixmap(userIcon));
        QWidget *pBubble = nullptr;
        if(type == "text"){
            pBubble = new TextBubble(role, msgList[i].content);
        }
        else if(type == "image"){
            pBubble = new PictureBubble(QPixmap(msgList[i].content) , role);
        }
        else if(type == "file"){

        }
        if(pBubble != nullptr){
            pChatItem->setWidget(pBubble);
            ui->chat_data_list->appendItem(pChatItem);
        }
    }
}

