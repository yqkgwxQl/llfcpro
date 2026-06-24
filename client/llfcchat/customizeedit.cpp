#include "customizeedit.h"
#include <QLineEdit>
#include <QAction>
#include <QIcon>
#include "chatdialog.h"

CustomizeEdit::CustomizeEdit(QWidget *parent)
    :QLineEdit(parent),_max_len(0){
    //QLineEdit::textChanged是用来通知的，if你修改了QLineEdit的内容，会自动在edit里进行显示 ； 事后通知你用textChanged ;对于setText()只是提供给你的接口用来修改edit里的内容的
    connect(this,&QLineEdit::textChanged,this,&CustomizeEdit::LimitMaxLen);
    QAction* search_action = new QAction(this); //绑定父类对象
    search_action->setIcon(QIcon(":/res/search.png"));
    this->addAction(search_action,QLineEdit::LeadingPosition);

    QAction* clear_action = new QAction(this);
    clear_action->setIcon(QIcon(":/res/close_transparent.png"));
    this->addAction(clear_action,QLineEdit::TrailingPosition);

    connect(this,&QLineEdit::textChanged,this,[this,clear_action](){
        if(this->text().size() > 0){
            clear_action->setIcon(QIcon(":/res/close_search.png"));
        }else{
            clear_action->setIcon(QIcon(":/res/close_transparent.png"));
        }
    });

    connect(clear_action,&QAction::triggered,[this,clear_action](){
        this->clear();
        clear_action->setIcon(QIcon(":/res/close_transparent.png"));
        this->clearFocus();
        //必须有对应的chatDialog实例才可以调用成员函数ShowSearch(bool)
        //解决方法：1.ChatDialog设置为单例 2.通过父窗口获取
        // 沿父控件链向上遍历，找到 ChatDialog
        // 沿父控件链向上遍历，找到 ChatDialog
        QWidget* parent = this->parentWidget();
        while (parent) {
            ChatDialog* chat_dlg = qobject_cast<ChatDialog*>(parent);
            if (chat_dlg) {
                chat_dlg->ShowSearch(false);
                return;
            }
            parent = parent->parentWidget();
        }
        qDebug() << "[CustomizeEdit]: ChatDialog not found in parent chain";

        // emit sig_clear_search();
    });
}

/*  -- dynamic_cast和static_cast的区别:
    static_cast = 编译时决定，不检查，你说是就是 dynamic_cast = 运行时检查，不是的话返回 nullptr/抛异常
        特性          static_cast         dynamic_cast
        检查时机          编译时             	运行时
        安全性     ❌ 不安全，转错了不报错	 ✅ 安全，转错了返回 nullptr
        性能	✅    零开销（编译时就决定）	 ❌ 有运行时开销（查虚函数表）
        适用转型	  任何已知安全的转型       只能用于多态类型（有虚函数的类）
        需要条件	  无特殊要求              类必须有至少一个虚函数
        指针失败	  —                     返回 nullptr
        引用失败	  —                     抛出 std::bad_cast 异常
    -- qobject_cast vs dynamic_cast 对比
        特性              qobject_cast            dynamic_cast
        依赖              Qt 元对象系统（MOC）       C++ RTTI
        速度              更快（查表）              较慢（遍历继承链）
        跨平台一致性      ✅ Qt 保证                 ❌ 不同编译器可能不同
        需要条件        类必须有 Q_OBJECT 宏         类必须有虚函数
        失败返回值       nullptr                 nullptr（指针）或异常（引用）
 */
void CustomizeEdit::SetMaxLength(std::size_t length){
    _max_len = length;
}

void CustomizeEdit::focusOutEvent(QFocusEvent *event){
    // 执行失去焦点时的处理逻辑
    //qDebug() << "CustomizeEdit focusout";
    // 调用基类的focusOutEvent()方法，保证基类的行为得到执行
    QLineEdit::focusOutEvent(event);
    //发送失去焦点得信号
    emit sig_foucus_out();
}

void CustomizeEdit::LimitMaxLen(QString text){
    if(_max_len <= 0){
        return;
    }
    QByteArray text_utf8 = text.toUtf8();
    std::size_t length = text_utf8.length();
    if(length > _max_len){
        text_utf8 = text_utf8.left(_max_len);       //left(n) 的意思是：取 QByteArray 中前 n 个字节
        this->setText(QString::fromUtf8(text_utf8));
    }
}