#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QIcon>
#include <tcpmgr.h>
#include <QScreen>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    this->setWindowTitle("llfcchat");
    this->setWindowIcon(QIcon(":/res/icon.ico"));
    _login_dlg = new LoginDialog(this);
    _login_dlg->setWindowFlags(Qt::CustomizeWindowHint | Qt::FramelessWindowHint);
    setCentralWidget(_login_dlg);

    //创建和注册消息链接 ——————> 仔细想想这里为什么要这样写？： _login_dlg -- QPushButton::clicked ——> switchRegister ————> MainWindow::SlotSwitchReg
    connect(_login_dlg,&LoginDialog::switchRegister,this,&MainWindow::SlotSwitchReg);
    //连接登录界面忘记密码信号
    connect(_login_dlg,&LoginDialog::switchReset,this,&MainWindow::SlotSwitchReset);
    //连接tcpmgr发送的sig_switch_chatdlg
    connect(TcpMgr::GetInstance().get(),&TcpMgr::sig_switch_chatdlg,this,&MainWindow::SlotSwitchChat);

    // _register_dlg = new RegisterDialog(this);

    // Qt::FramelessWindowHint：产生一个没有窗口边框、标题栏和系统按钮的窗口
    // Qt::CustomizeWindowHint：关闭默认的窗口标题提示，允许自定义窗口装饰*/
    // ============= 关于 setWindowFlags + setCentralWidget 的说明 =============
    //
    // 问题1：加不加 setWindowFlags 对显示尺寸有没有影响？
    // 答：没有。setCentralWidget() 内部会调用 setParent() 把 dialog 嵌入主窗口的中央区域，
    //     并自动拉伸到中央区域的大小。无论有没有 FramelessWindowHint，结果都一样。
    //
    // 问题2：加不加 setWindowFlags 对关闭行为有没有影响？
    // 答：没有。同上，setCentralWidget() 已经把 dialog 变成普通子控件了。
    //     子控件没有独立关闭按钮，关闭行为由主窗口控制。
    //
    // 那为什么还保留 setWindowFlags？
    // 如果在未来某个版本中，login_dlg 以独立窗口形式弹出（不经过 setCentralWidget），
    // 那么 FramelessWindowHint 才有实际意义——生成一个无边框的自定义登录窗口。
    // 目前这段代码里它确实不起作用，留作备用。
    // ========================================================================
    // _login_dlg->show();
    // emit TcpMgr::GetInstance()->sig_switch_chatdlg();
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::SlotSwitchReg(){
    _register_dlg = new RegisterDialog(this);
    _register_dlg->setWindowFlags(Qt::CustomizeWindowHint | Qt::FramelessWindowHint);
    setCentralWidget(_register_dlg);
    //连接注册界面返回登录信号
    connect(_register_dlg,&RegisterDialog::sigSwitchLogin,this,&MainWindow::SlotSwitchLogin);
    _login_dlg->hide();
    _register_dlg->show();
}

void MainWindow::SlotSwitchLogin(){
    _login_dlg = new LoginDialog(this);
    _login_dlg->setWindowFlags(Qt::CustomizeWindowHint | Qt::FramelessWindowHint);
    setCentralWidget(_login_dlg);
    connect(_login_dlg,&LoginDialog::switchRegister,this,&MainWindow::SlotSwitchReg);
    connect(_login_dlg,&LoginDialog::switchReset,this,&MainWindow::SlotSwitchReset);
    _register_dlg->hide();
    _login_dlg->show();
}

void MainWindow::SlotSwitchLogin_2(){
    _login_dlg = new LoginDialog(this);
    _login_dlg->setWindowFlags(Qt::CustomizeWindowHint | Qt::FramelessWindowHint);
    setCentralWidget(_login_dlg);
    connect(_login_dlg,&LoginDialog::switchRegister,this,&MainWindow::SlotSwitchReg);
    connect(_login_dlg,&LoginDialog::switchReset,this,&MainWindow::SlotSwitchReset);
    _reset_dlg->hide();
    _login_dlg->show();
}

void MainWindow::SlotSwitchChat(){
    _chat_dlg = new ChatDialog(this);
    _chat_dlg->setWindowFlags(Qt::CustomizeWindowHint | Qt::FramelessWindowHint);
    setCentralWidget(_chat_dlg);
    this->move((screen()->geometry().width() - 1050) / 2,
               (screen()->geometry().height() - 800) / 2);
    _chat_dlg->show();
    _login_dlg->hide();
    this->setMinimumSize(QSize(1050,700));
    //取消最大尺寸限制，让窗口可以无限放大。
    this->setMaximumSize(QWIDGETSIZE_MAX,QWIDGETSIZE_MAX);
}

void MainWindow::SlotSwitchReset(){
    _reset_dlg = new ResetDialog(this);
    _reset_dlg->setWindowFlags(Qt::CustomizeWindowHint | Qt::FramelessWindowHint);
    setCentralWidget(_reset_dlg);
    connect(_reset_dlg,&ResetDialog::switchLogin,this,&MainWindow::SlotSwitchLogin_2);
    _login_dlg->hide();
    _reset_dlg->show();
}

