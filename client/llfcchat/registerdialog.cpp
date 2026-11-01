#include "registerdialog.h"
#include "ui_registerdialog.h"
#include "global.h"
#include "httpmgr.h"
#include "clickedlabel.h"
#include <QRegularExpression>
#include <_mingw_mac.h>
#include <QRegularExpression>
#include <QTimer>
#include <QRandomGenerator>

RegisterDialog::RegisterDialog(QWidget *parent)
    : QDialog(parent),_countdown(5)
    , ui(new Ui::RegisterDialog)
{
    ui->setupUi(this);
    //1< pwdEdit,confirmEdit都设置为不显示  ------------------------
    ui->pwdEdit->setEchoMode(QLineEdit::Password);
    ui->confirmEdit->setEchoMode(QLineEdit::Password);
    /*指向界面上那个用来显示提示信息的标签控件（通常是一个 QLabel）。setProperty("state", "normal")：
    这是 Qt 的动态属性机制。它的作用是给这个控件设置（或修改）一个名为 "state" 的自定义属性，并将它的值设为 "normal"*/
    ui->errTip->setProperty("state","normal");
    repolish(ui->errTip);
    //.get ——> 单例(使用时自动创建唯一一个)，取原指针
    //2< 连接信号和槽  --------------------------
    connect(HttpMgr::GetInstance().get(),&HttpMgr::sig_reg_mod_finish,this,&RegisterDialog::slot_reg_mod_finish);
    initHttpHandlers();

    ui->errTip->clear();
    connect(ui->userEdit,&QLineEdit::editingFinished,this,[this](){
        checkUserValid();
    });
    connect(ui->emailEdit, &QLineEdit::editingFinished, this, [this](){
        checkEmailValid();
    });
    connect(ui->pwdEdit, &QLineEdit::editingFinished, this, [this](){
        checkPwdValid();
    });
    connect(ui->confirmEdit, &QLineEdit::editingFinished, this, [this](){
        checkConfirmValid();
    });
    connect(ui->varifyCodeEdit, &QLineEdit::editingFinished, this, [this](){
        checkVarifyValid();
    });
    // connect(ui->userEdit,&QLineEdit::editingFinished,this,&RegisterDialog::slot_checkUserValid);
    //3 < 密码是否可是标签的设置
        //1.把放到上面的标签设置为小手
    ui->pwdVisbleLabel->setCursor(Qt::PointingHandCursor);
    ui->confirmVisbleLabel->setCursor(Qt::PointingHandCursor);
        //2.设置6种状态
    ui->pwdVisbleLabel->SetState("invisible","invisible_hover","invisible_hover","visible","visible_hover","visible_hover");
    ui->confirmVisbleLabel->SetState("invisible","invisible_hover","invisible_hover","visible","visible_hover","visible_hover");

    //这里只有鼠标点击，并没有移动的逻辑
    connect(ui->pwdVisbleLabel,&ClickedLabel::clicked,[this](){
        auto state = ui->pwdVisbleLabel->GetCurState();
        if(state == ClickLbState::Normal){
            ui->pwdEdit->setEchoMode(QLineEdit::Password);
        }else{
            ui->pwdEdit->setEchoMode(QLineEdit::Normal);
        }
    });

    connect(ui->confirmVisbleLabel,&ClickedLabel::clicked,[this](){
        auto state = ui->confirmVisbleLabel->GetCurState();
        if(state == ClickLbState::Normal){
            ui->confirmEdit->setEchoMode(QLineEdit::Password);
        }else{
            ui->confirmEdit->setEchoMode(QLineEdit::Normal);
        }
    });

    // 4 < 创建定时器
    _countdown_timer = new QTimer(this);
    connect(_countdown_timer,&QTimer::timeout,[this](){
        if(_countdown <= 0){
            _countdown_timer->stop();
            _countdown = 5;
            emit sigSwitchLogin();
            return;
        }else{
            _countdown--;
            auto str = QString("注册成功，%1 s后返回登录").arg(_countdown);
            ui->tipLabel->setText(str);
        }
    });

    ui->getCode->setEnabled(false);
}

RegisterDialog::~RegisterDialog()
{
    qDebug() << "destruct RegisterDialog";
    delete ui;
}

// void RegisterDialog::on_getCode_clicked()
// {
//     //取出邮箱地址
//     QString email = ui->emailEdit->text();
//     //R"(...)"，这样在写正则表达式时就不用费劲地去给反斜杠 \ 做转义了
//     /*  \w : 字母、数字、下划线   ||   + : 至少一个
//         \.|_ : .或_   ||   ?: 0 or 1
//         * : 0 or 多
//         \.(\w+)+  : (.  字母、数字、下划线+)可出现多次
//     */
//     QRegularExpression regex(R"((\w+)(\.|_)?(\w*)@(\w+)(\.(\w+))+)");
//     /*regex.match(email)返回值：它不直接返回 true 或 false，而是返回一个 QRegularExpressionMatch 对象。
//         为什么？ 因为匹配的结果不仅仅是“成功”或“失败”。如果成功了，你可能还想知道“匹配到的内容是什么？”、“它在字符串的第几个位置？”、
//         “括号里捕获的内容是什么？”。这个 QRegularExpressionMatch 对象就是用来装这些详细信息的“容器”。
//     .hasMatch() —— 询问“战果”
//         作用：调用那个“容器”对象的 hasMatch() 函数。
//         返回值：这才是真正的 bool（true 或 false）。
//     含义：它只回答一个问题：“刚才的匹配成功了吗？”*/
//     bool match = regex.match(email).hasMatch();
//     if(match){
//         //发送http请求获取验证码
//         QJsonObject json_obj;
//         json_obj["email"] = email;
//         HttpMgr::GetInstance()->PostHttpReq(QUrl(gate_url_prefix + "/get_varifycode"),json_obj,
//                                             ReqId::ID_GET_VERIFY_CODE,Modules::REGISTERMOD);
//     }else{
//         //提示邮箱不正确
//         showTip(tr("邮箱地址不正确"),false);
//     }
// }

void RegisterDialog::on_getCode_clicked(){
    QString email("");
    if((email = checkEmailValid()) != ""){
        //发送http请求获取验证码
        QJsonObject json_obj;
        json_obj["email"] = email;
        HttpMgr::GetInstance()->PostHttpReq(QUrl(gate_url_prefix + "/get_varifycode"),json_obj,
                                            ReqId::ID_GET_VERIFY_CODE,Modules::REGISTERMOD);
    }
}


void RegisterDialog::slot_reg_mod_finish(ReqId id,QString res,ErrorCodes err){
    if(err != ErrorCodes::SUCCESS){
        showTip(tr("网络请求错误"),false);
        return;
    }
    //QString 内部是 UTF-16（Unicode），而 JSON 规范（RFC 8259）规定：JSON 文本在网络上必须以 UTF-8 编码传输，Qt 的 fromJson 也按 UTF-8 解读你给的字节
    //QJsonDocument ——> 可理解为 ... .json文件   res.toUtf8() ——> 把 QString 转换成标准的 QByteArray
    QJsonDocument jsonDoc = QJsonDocument::fromJson(res.toUtf8());
    //json解析错误
    if(jsonDoc.isNull()){
        showTip(tr("json解析失败"),false);
        return;
    }
    if(!jsonDoc.isObject()){
        showTip(tr("json解析失败"),false);
        return;
    }
    QJsonObject jsonObj = jsonDoc.object();
    //回调
    _handlers[id](jsonObj);
    return;
}

void RegisterDialog::on_confirmBtn_clicked()
{
    if(checkUserValid() && checkPwdValid() && checkConfirmValid() && checkVarifyValid() && (checkEmailValid()!= "")){
        //发送http请求注册用户
        QJsonObject json_obj;
        json_obj["user"] = ui->userEdit->text();
        json_obj["email"] = ui->emailEdit->text();
        json_obj["passwd"] = xorString(ui->pwdEdit->text());
        //---------------------------------？？？？？？？？？？？？这里未来一定会改进
        json_obj["sex"] = 0;
        int randomValue = QRandomGenerator::global()->bounded(100);
        int head_i = randomValue % heads.size();
        json_obj["icon"] = heads[head_i];
        json_obj["nick"] = ui->userEdit->text();
        //---------------------------------？？？？？？？？？？？？
        json_obj["confirm"] = xorString(ui->confirmEdit->text());
        json_obj["varifycode"] = ui->varifyCodeEdit->text();
        HttpMgr::GetInstance()->PostHttpReq(QUrl(gate_url_prefix + "/user_register"),
                                            json_obj,ReqId::ID_REG_USER,Modules::REGISTERMOD);
    }
}

void RegisterDialog::initHttpHandlers()
{
    //注册获取验证吗回包的逻辑
    _handlers.insert(ReqId::ID_GET_VERIFY_CODE,[this](const QJsonObject& jsonObj){
        int err = jsonObj["error"].toInt();
        if(err != ErrorCodes::SUCCESS){
            showTip(tr("参数错误"),false);
            return;
        }
        auto email = jsonObj["email"].toString();
        //服务器会发送验证码的
        showTip(tr("验证码已经发送到邮箱，注意查收"),true);
        qDebug() << "email is " << email;
    });

    _handlers.insert(ReqId::ID_REG_USER,[this](const QJsonObject& jsonObj){
        int err = jsonObj["error"].toInt();
        if(err != ErrorCodes::SUCCESS){
            showTip(tr("参数错误"),false);
            return;
        }
        auto email = jsonObj["email"].toString();
        //服务器会发送验证码的
        showTip(tr("用户注册成功"),true);
        qDebug() << "user uuid is " << jsonObj["uid"].toString();
        qDebug() << "email is " << email;

        ChangeTipPage();
    });
}

void RegisterDialog::showTip(QString tip,bool b_ok){
    if(!b_ok){
        ui->errTip->setProperty("state","err");
    }else{
        ui->errTip->setProperty("state","normal");
    }
    ui->errTip->setText(tip);
    repolish(ui->errTip);
}

void RegisterDialog::AddTipErr(TipErr tip_err, QString tip){
    _tip_errs[tip_err] = tip;
    showTip(tip,false);
}

void RegisterDialog::DelTipErr(TipErr tip_err){
    _tip_errs.remove(tip_err);
    if(_tip_errs.empty()){
        ui->errTip->clear();
        return;
    }else
        showTip(_tip_errs.first(),false);
}

bool RegisterDialog::checkUserValid(){
    /*
      ^\s*$：
    ^:从字符串的起始位置开始匹配
    \s:任意空白字符 ： 空格、\t(tabel)、\r(回车)、\n(换行)
    *:零次或多次 —— 前面的\s 可以出现 0 次或无数次
    $:结尾 —— 匹配到字符串的结束位置

    为什么普通字符里反斜杠 \ 要进行转义？
    因为在 C++（以及 C、Java、Python 等很多语言）中，反斜杠 \ 是转义字符——它有特殊含义，不代表它自己。
        1.eg:QString s = "他说："你好"";   // ❌ 编译错误！第二个"被理解为字符串结束
    所以需要用 \" 告诉编译器："这个双引号不是字符串结尾，它就是一个普通的引号字符"。
          QString s = "他说：\"你好\"";  // ✅ 正确

        反斜杠 \ 被用作"转义标记"，它后面的字符会被组合成一个特殊的含义：

        你写的	实际代表的字符         说明
         \n      换行符             不是字母 n
         \t      制表符             不是字母 t
         \"      普通双引号         不是字符串结束
         \\      普通反斜杠	      就是字面上的一个反斜杠

        2.eg:原始正则：     ^\s*$
        C++ 字符串：   "^\\s*$"       ← 多了一层转义
        更复杂的：     "\\(\\w+\\)"   ← 眼睛花了
        用 R"(...)":   R"(\(\w+\))"   ← 清爽！
    R"(...)" 就是让你写正则或路径时，不用再跟反斜杠做斗争——直接写你真正想要的内容就行。
    */
    QRegularExpression regex(R"(^\s*$)");
    if(regex.match(ui->userEdit->text()).hasMatch()){
        AddTipErr(TipErr::TIP_USER_ERR,tr("用户名不能为空或者含有空白字符"));
        return false;
    }
    DelTipErr(TipErr::TIP_USER_ERR);
    return true;
}

QString RegisterDialog::checkEmailValid(){
    QString email = ui->emailEdit->text();
    /*
        ①(\w+): \w：字母、数字、下划线； +：一个或多个
        ②(\.|_)?: \.:.； ?: 0或1次   .或者_作为分割符
        ③ *:0或多个
            ①②③+@合起来匹配：john@、john.doe@、john_doe@、a.b@
        ④(\w+)
        ⑤(\.(\w+))+ ： (. + 字母数字下划线(多个))多个
        ④⑤ ：qq.com、163.com、outlook.com.cn
    */
    QRegularExpression regex(R"((\w+)(\.|_)?(\w*)@(\w+)(\.(\w+))+)");
    bool match = regex.match(email).hasMatch();
    if(!match){
        AddTipErr(TipErr::TIP_EMAIL_ERR,tr("邮箱地址不正确"));
        ui->getCode->setEnabled(false);
        return "";
    }else{
        DelTipErr(TipErr::TIP_EMAIL_ERR);
        ui->getCode->setEnabled(true);
        return email;
    }
}

bool RegisterDialog::checkPwdValid(){
//     ^ [a-zA-Z0-9!@#$%^&*.] {6,15} $
//     ↑   └──────┬──────┘   └─┬──┘ ↑
//    开头     允许的字符      长度  结尾
    QString pwd = ui->pwdEdit->text();
    if(pwd.length() < 6 || pwd.length()>15){
        //提示长度不准确
        AddTipErr(TipErr::TIP_PWD_ERR, tr("密码长度应为6~15"));
        return false;
    }
    QRegularExpression regex(R"(^[a-zA-Z0-9!@#$%^&*.]{6,15}$)");
    bool match = regex.match(pwd).hasMatch();
    if(!match){
        AddTipErr(TipErr::TIP_PWD_ERR,tr("密码里不能包含非法字符"));
        return false;
    }else{
        DelTipErr(TipErr::TIP_PWD_ERR);
        return true;
    }
}

bool RegisterDialog::checkConfirmValid(){
    //对于confirmPwd自身进行检查，防止有用户先填confirmPwd
    QString confirmPwd = ui->pwdEdit->text();
    if(confirmPwd.length() < 6 || confirmPwd.length()>15){
        //提示长度不准确
        AddTipErr(TipErr::TIP_CONFIRM_ERR, tr("长度应为6~15"));
        return false;
    }
    QRegularExpression regex(R"(^[a-zA-Z0-9!@#$%^&*.]{6,15}$)");
    bool match = regex.match(confirmPwd).hasMatch();
    if(!match){
        AddTipErr(TipErr::TIP_CONFIRM_ERR,tr("确认里不能包含非法字符"));
        return false;
    }
    //接下来查看是否confirmPwd和pwd一致
    QString pwd = ui->pwdEdit->text();
    if(confirmPwd != pwd){
        AddTipErr(TipErr::TIP_CONFIRM_ERR,tr("确认和密码不一致"));
        return false;
    }
    DelTipErr(TipErr::TIP_CONFIRM_ERR);
    return true;
}

bool RegisterDialog::checkVarifyValid(){
    QString verifyCode = ui->varifyCodeEdit->text();
    if(verifyCode.isEmpty()){
        AddTipErr(TipErr::TIP_VARIFY_ERR, tr("验证码不能为空"));
        return false;
    }

    DelTipErr(TipErr::TIP_VARIFY_ERR);
    return true;
}

void RegisterDialog::ChangeTipPage(){
    _countdown_timer->stop();
    //stackedWidget 是一个堆叠窗口
    ui->stackedWidget->setCurrentWidget(ui->page_2);
    _countdown_timer->start(1000);
}

void RegisterDialog::on_pushButton_clicked(){
    _countdown_timer->stop();
    _countdown = 5;
    emit sigSwitchLogin();
}

