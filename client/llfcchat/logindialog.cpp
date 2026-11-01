#include "logindialog.h"
#include "ui_logindialog.h"
#include <QRegularExpression>
#include <QJsonObject>
#include "httpmgr.h"
#include "QUrl"
#include "global.h"
#include <QPainter>
#include <QPainterPath>
#include "tcpmgr.h"

LoginDialog::LoginDialog(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::LoginDialog)
{
    ui->setupUi(this);
    //1< 设置忘记密码标签的样式，密码隐式显示。同时连接对应的信号和槽  ------------------
    ui->forgetLable->SetState("normal","hover","","selected","selected_hover","");
    ui->forgetLable->setCursor(Qt::PointingHandCursor);
    connect(ui->forgetLable, &ClickedLabel::clicked, this, &LoginDialog::slot_forget_pwd);
    ui->pwdEdit->setEchoMode(QLineEdit::Password);
    connect(ui->pwdEdit,&QLineEdit::editingFinished,[this](){
        checkPwdValid();
    });
    connect(ui->emailEdit,&QLineEdit::editingFinished,[this](){
        checkEmailValid();
    });

    // 注意：这里的LoginDialog::switchRegister 是信号！！！
    connect(ui->registerBtn,&QPushButton::clicked,this,&LoginDialog::switchRegister);
    connect(HttpMgr::GetInstance().get(),&HttpMgr::sig_login_mod_finish,this,&LoginDialog::slot_login_mod_finish);
    //2< 涉及tcp的两个信号与槽
    connect(this,&LoginDialog::sig_connect_tcp,TcpMgr::GetInstance().get(),&TcpMgr::slot_tcp_connect);
    connect(TcpMgr::GetInstance().get(),&TcpMgr::sig_con_success,this,&LoginDialog::slot_tcp_con_finish);
    //3< 初始化登录界面的头像，初始化http的login回调函数 -------------------
    initHead();
    initHttpHandlers();
}

LoginDialog::~LoginDialog(){
    qDebug() << "destruct LoginDialog";
    delete ui;
}

void LoginDialog::slot_forget_pwd(){
    qDebug()<<"slot forget pwd";
    emit switchReset();
}


void LoginDialog::initHttpHandlers(){
    // auto self = enable_shared_from_this();
    //这里为什么用this,不用self,因为initHttpHandlers在构造函数内部，此时此对象并未构造完毕，因此不能使用智能指针
    _handlers.insert(ReqId::ID_LOGIN_USER,[this](const QJsonObject& jsonObj){
        int error = jsonObj["error"].toInt();
        if(error != ErrorCodes::SUCCESS){
            showTip(tr("[_handlers.insert(ReqId::ID_LOGIN_USER,]:参数错误"),false);
            enableBtn(true);
            return;
        }
        auto user = jsonObj["user"].toString();
        //发送信号通知tcpMgr发送长链接
        ServerInfo si;
        si.Uid = jsonObj["uid"].toInt();
        si.Host = jsonObj["host"].toString();
        si.Port = jsonObj["port"].toString();
        si.Token = jsonObj["token"].toString();

        _uid = si.Uid;
        _token = si.Token;
        //前两个是用户的，后三个是服务器的，最后一个也可以说是用户的
        qDebug()<< "user is " << user
                 << " uid is " << si.Uid
                 <<" host is "<< si.Host
                 << " Port is " << si.Port
                 << " Token is " << si.Token;
        //这里获取到可用的chatServer的信息，然后发送给TcpMgr，让它真正连接 但是TcpMgr是单例的呀，只有一个_socket，怎么能负责多个客户端的连接呢？？
            //因为这里就是客户端，只有一个客户端
        emit sig_connect_tcp(si);
    });
}

void LoginDialog::slot_login_mod_finish(ReqId req_id,QString res,ErrorCodes err){
    if(err != ErrorCodes::SUCCESS){
        showTip(tr("网络请求错误"),false);
        return;
    }

    QJsonDocument jsonDoc = QJsonDocument::fromJson(res.toUtf8());
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
    _handlers[req_id](jsonObj);
    return;
}

void LoginDialog::slot_tcp_con_finish(bool bsuccess){
    if(bsuccess){
        /*我的问题是：这里client不是已经和chatServer建立好链接了吗，为什么还要再发一个登录请求？ ————> TCP 连接成功只说明一件事：你和那台服务器的网络是通的。
        但 ChatServer 还不知道你是谁
        TCP 连接成功时，ChatServer 只知道：
            "有个程序的 socket 连上来了，它来自 IP 192.168.1.100:54321"
            但它不知道：
            - 这个连接对应的是 uid=42 的张三
            - 这个连接携带的 token 是否有效
            - 这个用户是不是已经在别处登录了，需要踢掉
        接下来所发送给聊天服务器的登录请求则是会再chatServer进行验证用户身份是否合法*/
        showTip(tr("聊天服务连接成功，正在登录..."),true);
        QJsonObject jsonObj;
        jsonObj["uid"] = _uid;
        jsonObj["token"] = _token;
        //QJsonDocument::Indented 表示使用缩进模式。效果：生成的 JSON 字符串会带有换行符和空格缩进，非常便于人类阅读、日志记录或调试排查
        QJsonDocument jsonDoc(jsonObj);
        //序列化
        QByteArray jsonBytes = jsonDoc.toJson(QJsonDocument::Indented);
        //发送http请求给chatServer
        //QObject::connect(this,&TcpMgr::sig_send_data,this,&TcpMgr::slot_send_data);
        emit TcpMgr::GetInstance()->sig_send_data(ReqId::ID_CHAT_LOGIN,jsonBytes);
        /* 为什么不直接TcpMgr::GetInstance()->slot_send_data(ReqId::ID_CHAT_LOGIN,jsonString);
        假设将来某天代码重构，slot_tcp_con_finish 在子线程中被调用了：
         -- 未来可能在子线程中：
            void SomeClass::someFunction() {
                // 在子线程中
                slot_tcp_con_finish(true);
            }
        -- 写法 B：直接调 ❌ 隐患
        TcpMgr::GetInstance()->slot_send_data(...);
        // _socket.write() 在子线程执行
        // QTcpSocket 不是线程安全的，可能崩溃

        // 写法 A：emit ✅ 安全
        emit TcpMgr::GetInstance()->sig_send_data(...);
        // Qt 发现：发送方在子线程，接收方在主线程
        // 自动通过事件队列投递到主线程执行
        // _socket.write() 在主线程执行，安全
        -- emit：不管当前在哪里调用，_socket.write() 都在正确的线程执行
        emit TcpMgr::GetInstance()->sig_send_data(...);

        // 直接调：如果当前在子线程，_socket.write() 可能崩溃
        TcpMgr::GetInstance()->slot_send_data(...);
         */
    }else{
        showTip(tr("网络请求错误2"),false);
        enableBtn(true);
        return;
    }
}

void LoginDialog::slot_login_failed(int err){
    QString result = QString("登录失败, err is %1").arg(err);
    showTip(result,false);
    enableBtn(true);
}

//它一次性把登录、注册两个按钮设为可用/不可用。典型用法是"发起请求前禁用、请求失败后恢复"，防止用户在请求还没回来时连点按钮，造成重复的 HTTP 请求
bool LoginDialog::enableBtn(bool enabled){
    ui->loginBtn->setEnabled(enabled);
    ui->registerBtn->setEnabled(enabled);
    return true;
}

void LoginDialog::on_loginBtn_clicked(){
    enableBtn(false);
    qDebug()<<"login btn clicked";
    if(checkPwdValid() && (checkEmailValid() != "")){
        QString email = ui->emailEdit->text();
        QString pwd = ui->pwdEdit->text();
        QJsonObject json_obj;
        json_obj["email"] = email;
        json_obj["passwd"] = xorString(pwd);
        HttpMgr::GetInstance()->PostHttpReq(QUrl(gate_url_prefix+"/user_login"),json_obj,ReqId::ID_LOGIN_USER,Modules::LOGINMOD);
    }
}

void LoginDialog::initHead(){
    // 加载图片
    QPixmap originalPixmap(":/res/head_1.jpg");
    // 设置图片自动缩放
    qDebug()<< originalPixmap.size() << ui->head_label->size();
    //Qt::KeepAspectRatio  保持宽高比  Qt::SmoothTransformation 使用平滑变换算法（如双线性插值），让缩放后的图片边缘更清晰，不会出现锯齿或马赛克。
    // originalPixmap = originalPixmap.scaled(ui->head_label->size(),Qt::KeepAspectRatio, Qt::SmoothTransformation);
    originalPixmap = originalPixmap.scaled(QSize(180,180),Qt::KeepAspectRatio, Qt::SmoothTransformation);

    // 创建一个和原始图片相同大小的QPixmap，用于绘制圆角图片
    QPixmap roundedPixmap(originalPixmap.size());
    roundedPixmap.fill(Qt::transparent); // 用透明色填充

    QPainter painter(&roundedPixmap);
    painter.setRenderHint(QPainter::Antialiasing); // 设置抗锯齿，使圆角更平滑
    painter.setRenderHint(QPainter::SmoothPixmapTransform); //平滑算法

    // 使用QPainterPath设置圆角
    QPainterPath path;
    //四个参数定义了矩形的范围（即整张图片的大小）。  最后两个参数 10, 10 定义了圆角的半径（x轴半径和y轴半径）。数值越大，角越圆。
    path.addRoundedRect(0, 0, originalPixmap.width(), originalPixmap.height(), 10, 10); // 最后两个参数分别...
    //这是最关键的一句。它的意思是：“画家，从现在开始，你只能在这个路径（圆角矩形）范围内作画，超出这个范围的都不要画”。这就相当于制作了一个“模具”。
    painter.setClipPath(path);

    // 将原始图片绘制到roundedPixmap上
    painter.drawPixmap(0, 0, originalPixmap);

    // 设置绘制好的圆角图片到QLabel上
    ui->head_label->setPixmap(roundedPixmap);
}

void LoginDialog::showTip(QString tip,bool b_ok){
    if(!b_ok){
        ui->errTip->setProperty("state","err");
    }else{
        ui->errTip->setProperty("state","normal");
    }
    ui->errTip->setText(tip);
    repolish(ui->errTip);
}

void LoginDialog::AddTipErr(TipErr tip_err, QString tip){
    _tip_errs[tip_err] = tip;
    showTip(tip,false);
}

void LoginDialog::DelTipErr(TipErr tip_err){
    _tip_errs.remove(tip_err);
    if(_tip_errs.empty()){
        ui->errTip->clear();
        return;
    }else
        showTip(_tip_errs.first(),false);
}

QString LoginDialog::checkEmailValid(){
    QString email = ui->emailEdit->text();
    QRegularExpression regex(R"((\w+)(\.|_)?(\w*)@(\w+)(\.(\w+))+)");
    bool match = regex.match(email).hasMatch();
    if(!match){
        AddTipErr(TipErr::TIP_EMAIL_ERR,tr("邮箱地址不正确"));
        return "";
    }else{
        DelTipErr(TipErr::TIP_EMAIL_ERR);
        return email;
    }
}

bool LoginDialog::checkPwdValid(){
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





