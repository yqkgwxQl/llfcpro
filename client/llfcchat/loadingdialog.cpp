#include "loadingdialog.h"
#include "ui_loadingdialog.h"
#include <QMovie>

LoadingDialog::LoadingDialog(QWidget *parent, QString tip)
    : QDialog(parent)
    , ui(new Ui::LoadingDialog)
{
    ui->setupUi(this);
    //1. 让这个 Widget 透明背景、无边框、拦截底部事件
    //  | Qt::WindowSystemMenuHint 提示窗口系统显示系统菜单 Qt::WindowStaysOnTopHint 提示窗口系统该窗口应该始终置顶
    setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint| Qt::WindowStaysOnTopHint);
    setAttribute(Qt::WA_TranslucentBackground); //设置背景透明
    // 2. 让它覆盖父窗口整个面积
    if (parent) {
        // 获取屏幕尺寸
        setFixedSize(parent->size()); // 设置对话框为全屏尺寸
    }
    if (parent) {
        QPoint topLeft = parent->mapToGlobal(QPoint(0, 0)); //父窗口左上角的坐标 (0,0) 转换为全局屏幕坐标。因为子窗口的坐标是相对于父窗口的，
                                                            //而 move() 函数通常需要全局坐标来准确定位。
        move(topLeft);      //将加载弹窗移动到父窗口的左上角。结合上面的 setFixedSize，这就实现了完美覆盖父窗口的效果。
    }

    QMovie* movie = new QMovie(":/res/loading.gif");
    ui->loading_lb->setMovie(movie);
    movie->start();
    //告诉 QMovie：将解码后的每一帧缩放到 label大小
    movie->setScaledSize(ui->loading_lb->size());
    ui->status_lb->setText(tip);
}

LoadingDialog::~LoadingDialog()
{
    delete ui;
}
