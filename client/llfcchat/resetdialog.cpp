#include "resetdialog.h"
#include "ui_resetdialog.h"

ResetDialog::ResetDialog(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::ResetDialog)
{
    ui->setupUi(this);
    connect(ui->userEdit,&QLineEdit::editingFinished,this,[this](){
        checkUserValid();
    });

    connect(ui->emailEdit, &QLineEdit::editingFinished, this, [this](){
        checkEmailValid();
    });

    connect(ui->newPwdEdit, &QLineEdit::editingFinished, this, [this](){
        checkPwdValid();
    });


    connect(ui->varifyCodeEdit, &QLineEdit::editingFinished, this, [this](){
        checkVarifyValid();
    });

    ui->getCode->setEnabled(false);
    connect(HttpMgr::GetInstance().get(),&HttpMgr::sig_reset_mod_finish,this,&ResetDialog::slot_reset_mod_finish);
    initHttpHandlers();
}

ResetDialog::~ResetDialog()
{
    delete ui;
}



void ResetDialog::showTip(QString tip,bool b_ok){
    if(!b_ok){
        ui->errTip->setProperty("state","err");
    }else{
        ui->errTip->setProperty("state","normal");
    }
    ui->errTip->setText(tip);
    repolish(ui->errTip);
}

void ResetDialog::AddTipErr(TipErr tip_err, QString tip){
    _tip_errs[tip_err] = tip;
    showTip(tip,false);
}

void ResetDialog::DelTipErr(TipErr tip_err){
    _tip_errs.remove(tip_err);
    if(_tip_errs.empty()){
        ui->errTip->clear();
        return;
    }else
        showTip(_tip_errs.first(),false);
}

bool ResetDialog::checkUserValid(){
    QRegularExpression regex(R"(^\s*$)");
    if(regex.match(ui->userEdit->text()).hasMatch()){
        AddTipErr(TipErr::TIP_USER_ERR,tr("用户名不能为空或者含有空白字符"));
        return false;
    }
    DelTipErr(TipErr::TIP_USER_ERR);
    return true;
}

QString ResetDialog::checkEmailValid(){
    QString email = ui->emailEdit->text();
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

bool ResetDialog::checkPwdValid(){
    QString pwd = ui->newPwdEdit->text();
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

bool ResetDialog::checkVarifyValid(){
    QString verifyCode = ui->varifyCodeEdit->text();
    if(verifyCode.isEmpty()){
        AddTipErr(TipErr::TIP_VARIFY_ERR, tr("验证码不能为空"));
        return false;
    }

    DelTipErr(TipErr::TIP_VARIFY_ERR);
    return true;
}

void ResetDialog::on_getCode_clicked(){
    QString email("");
    if((email = checkEmailValid()) != ""){
        //发送http请求获取验证码
        qDebug() << "getCode has been clicked";
        QJsonObject json_obj;
        json_obj["email"] = email;
        HttpMgr::GetInstance()->PostHttpReq(QUrl(gate_url_prefix + "/get_varifycode"),json_obj,
                                            ReqId::ID_GET_VERIFY_CODE,Modules::REGISTERMOD);
    }
}

void ResetDialog::slot_reset_mod_finish(ReqId req_id, QString res, ErrorCodes err){
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

void ResetDialog::on_confirmBtn_clicked(){
    if(checkUserValid() && checkPwdValid() && checkVarifyValid() && (checkEmailValid()!= "")){
        //发送http请求注册用户
        QJsonObject json_obj;
        json_obj["user"] = ui->userEdit->text();
        json_obj["email"] = ui->emailEdit->text();
        json_obj["passwd"] = xorString(ui->newPwdEdit->text());
        json_obj["varifycode"] = ui->varifyCodeEdit->text();
        HttpMgr::GetInstance()->PostHttpReq(QUrl(gate_url_prefix + "/reset_pwd"),
                                            json_obj,ReqId::ID_RESET_PWD,Modules::RESETMOD);
    }
}


void ResetDialog::initHttpHandlers(){
    //注册获取验证吗回包的逻辑
    _handlers.insert(ReqId::ID_GET_VERIFY_CODE,[this](const QJsonObject& jsonObj){
        int err = jsonObj["error"].toInt();
        if(err != ErrorCodes::SUCCESS){
            showTip(tr("[_handlers.insert(ReqId::ID_GET_VERIFY_CODE,)]:参数错误"),false);
            return;
        }
        auto email = jsonObj["email"].toString();
        //服务器会发送验证码的
        showTip(tr("验证码已经发送到邮箱，注意查收"),true);
        qDebug() << "email is " << email;
    });

    _handlers.insert(ReqId::ID_RESET_PWD,[this](const QJsonObject& jsonObj){
        int err = jsonObj["error"].toInt();
        if(err != ErrorCodes::SUCCESS){
            showTip(tr("[_handlers.insert(ReqId::ID_RESET_PWD,]:参数错误"),false);
            return;
        }
        auto email = jsonObj["email"].toString();
        showTip(tr("重置成功,点击返回登录"), true);
        qDebug()<< "email is " << email ;
        qDebug()<< "user uuid is " <<  jsonObj["uuid"].toString();
    });
}


void ResetDialog::on_returnBtn_clicked(){
    emit switchLogin();
}

