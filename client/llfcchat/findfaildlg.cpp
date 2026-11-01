#include "findfaildlg.h"
#include "ui_findfaildlg.h"

FindFailDlg::FindFailDlg(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::FindFailDlg)
{
    ui->setupUi(this);
    setWindowTitle("添加");
    //隐藏对话标题栏
    setWindowFlags(windowFlags() | Qt::FramelessWindowHint);    //无边框
    this->setObjectName("FindFaildDlg");
    ui->fail_sure_btn->SetState("normal","hover","press");
    this->setModal(true);
}

FindFailDlg::~FindFailDlg()
{
    delete ui;
}

void FindFailDlg::on_fail_sure_btn_clicked()
{
    this->hide();
}

