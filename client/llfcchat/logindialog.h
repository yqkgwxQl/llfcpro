#ifndef LOGINDIALOG_H
#define LOGINDIALOG_H

#include <QDialog>
#include "global.h"
#include <QMap>

namespace Ui {
class LoginDialog;
}

class LoginDialog : public QDialog
{
    Q_OBJECT

public:
    explicit LoginDialog(QWidget *parent = nullptr);
    ~LoginDialog();
public slots:
    void slot_forget_pwd();
signals:
    void switchRegister();
    void switchReset();
    void sig_connect_tcp(ServerInfo si);
private slots:
    void on_loginBtn_clicked();
    void slot_login_mod_finish(ReqId req_id,QString res,ErrorCodes err);
    void slot_tcp_con_finish(bool bsuccess);
    void slot_login_failed(int err);
private:
    void initHttpHandlers();
    QMap<ReqId,std::function<void(const QJsonObject&)>> _handlers;
    void initHead();
    void showTip(QString tip,bool b_ok);
    QString checkEmailValid();
    bool checkPwdValid();
    bool enableBtn(bool);
    QMap<TipErr,QString> _tip_errs;
    void AddTipErr(TipErr tip_err, QString tip);
    void DelTipErr(TipErr tip_err);

    int _uid;
    QString _token;
    Ui::LoginDialog *ui;
};

#endif // LOGINDIALOG_H
