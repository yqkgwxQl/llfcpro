#ifndef REGISTERDIALOG_H
#define REGISTERDIALOG_H

#include <QDialog>
#include <QMap>
#include <QJsonObject>
#include <functional>
#include "global.h"
// #include <QQueue>
#include <QTimer>

namespace Ui {
class RegisterDialog;
}

class RegisterDialog : public QDialog
{
    Q_OBJECT

public:
    explicit RegisterDialog(QWidget *parent = nullptr);
    ~RegisterDialog();
signals:
    void sigSwitchLogin();
private slots:
    void on_getCode_clicked();
    void slot_reg_mod_finish(ReqId req_id,QString res,ErrorCodes err);
    void on_confirmBtn_clicked();
    // void slot_checkUserValid();
    void on_pushButton_clicked();

private:
    //引用常量，不能用左值引用
    // void showTip(QString& email_path);
    void initHttpHandlers();
    QMap<ReqId,std::function<void(const QJsonObject&)>> _handlers;
    QMap<TipErr,QString> _tip_errs;
    // QQueue<TipErr,QString> _tip_errs;
    void showTip(QString tip,bool b_ok);
    void AddTipErr(TipErr tip_err, QString tip);
    void DelTipErr(TipErr tip_err);

    bool checkUserValid();
    QString checkEmailValid();
    bool checkPwdValid();
    bool checkConfirmValid();
    bool checkVarifyValid();

    QTimer* _countdown_timer;
    int _countdown;
    void ChangeTipPage();

    Ui::RegisterDialog *ui;
};

#endif // REGISTERDIALOG_H
