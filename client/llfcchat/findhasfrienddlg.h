#ifndef FINDHASFRIENDDLG_H
#define FINDHASFRIENDDLG_H

#include <QDialog>

namespace Ui {
class FindHasFriendDlg;
}

class FindHasFriendDlg : public QDialog
{
    Q_OBJECT

public:
    explicit FindHasFriendDlg(QWidget *parent = nullptr);
    ~FindHasFriendDlg();

private slots:
    void on_fail_sure_btn_clicked();

private:
    Ui::FindHasFriendDlg *ui;
};

#endif // FINDHASFRIENDDLG_H
