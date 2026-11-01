#include "findhasfrienddlg.h"
#include "ui_findhasfrienddlg.h"

FindHasFriendDlg::FindHasFriendDlg(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::FindHasFriendDlg)
{
    ui->setupUi(this);
    setWindowTitle("添加");
    //隐藏对话标题栏
    setWindowFlags(windowFlags() | Qt::FramelessWindowHint);    //无边框
    this->setObjectName("FindHasFriendDlg");
    ui->fail_sure_btn->SetState("normal","hover","press");
    this->setModal(true);
}

FindHasFriendDlg::~FindHasFriendDlg()
{
    delete ui;
}

void FindHasFriendDlg::on_fail_sure_btn_clicked()
{
    this->hide();
}

