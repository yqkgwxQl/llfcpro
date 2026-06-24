#ifndef RESETDIALOG_H
#define RESETDIALOG_H

#include <QDialog>
#include "httpmgr.h"

//--------------------------------其实由于ResetDialog和RegiseterDialog重合的地方太多，完全可以创建基类进行复用-----------
namespace Ui {
class ResetDialog;
}

class ResetDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ResetDialog(QWidget *parent = nullptr);
    ~ResetDialog();
signals:
    void switchLogin();
private slots:
    void on_getCode_clicked();
    void on_returnBtn_clicked();

    void on_confirmBtn_clicked();
    void slot_reset_mod_finish(ReqId req_id,QString res,ErrorCodes err);
private:
    void initHttpHandlers();
    void showTip(QString tip,bool b_ok);
    QMap<ReqId,std::function<void(const QJsonObject&)>> _handlers;
    QMap<TipErr,QString> _tip_errs;
    void AddTipErr(TipErr tip_err, QString tip);
    void DelTipErr(TipErr tip_err);

    bool checkUserValid();
    QString checkEmailValid();
    bool checkPwdValid();
    bool checkVarifyValid();

    Ui::ResetDialog *ui;
};

#endif // RESETDIALOG_H
