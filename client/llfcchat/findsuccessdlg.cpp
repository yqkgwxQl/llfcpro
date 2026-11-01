#include "findsuccessdlg.h"
#include "ui_findsuccessdlg.h"
#include <QDir>
#include "applyfriend.h"

FindSuccessDlg::FindSuccessDlg(QWidget *parent)
    : QDialog(parent),_parent(parent) ,ui(new Ui::FindSuccessDlg){
    ui->setupUi(this);
    // 设置对话框标题
    setWindowTitle(tr("添加"));
    // 隐藏对话框标题栏
    this->setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);
    // 获取当前应用程序的路径
    QString app_path = QCoreApplication::applicationDirPath();
    QString pix_path = QDir::toNativeSeparators(app_path + QDir::separator() + "static"+QDir::separator()+"head_1.jpg");
    QPixmap pixmap(pix_path);
    QPixmap scale_pixmap = pixmap.scaled(ui->head_lb->size(),Qt::KeepAspectRatio , Qt::SmoothTransformation);
    ui->head_lb->setPixmap(scale_pixmap);
    ui->add_friend_btn->SetState("normal","hover","press");
    this->setModal(true);
}

FindSuccessDlg::~FindSuccessDlg(){
    delete ui;
}

void FindSuccessDlg::setSearchInfo(std::shared_ptr<SearchInfo> search_info){
    ui->name_lb->setText(search_info->_name);
    _si = search_info;
}

void FindSuccessDlg::on_add_friend_btn_clicked(){
    this->hide();
    //弹出加好友界面
    auto applyFriend = new ApplyFriend(_parent);
    applyFriend->SetSearchInfo(_si);
    applyFriend->setModal(true);
    applyFriend->show();
}
