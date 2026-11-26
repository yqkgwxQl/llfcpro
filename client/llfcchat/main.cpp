#include "mainwindow.h"

#include <QApplication>
#include <QFile>
#include <QDebug>
#include <QLatin1String>
#include <QString>
#include <QDir>
#include <QSettings>
#include "logger.h"
#include "theme/ThemeManager.h"

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    Logger::GetInstance();

    QCoreApplication::setOrganizationName("llfc");
    QCoreApplication::setApplicationName("llfcchat");   //%APPDATA%\llfc\llfcchat
    // ==================== ① 主题：先加载，再应用 ====================
    // load()  ：读 %APPDATA%/.../theme.ini；文件不存在或键缺失 → 用 defaultTokens()
    // apply() ：把 @token@ 替换成真实颜色，然后 qApp->setStyleSheet(...)
    // 必须在 MainWindow 构造之前调用，否则窗口先按系统默认样式显示、再被重绘，会闪一下。

    ThemeManager::GetInstance()->load();

    ThemeManager::GetInstance()->apply();
    // 模板缺失时 buildQss() 返回空并打 warning，界面退化成系统默认样式（不再另存一份 QSS 兜底）

    // ==================== ② 配置：不再依赖 QSS 是否加载成功 ====================
    //这里使用绝对路径
    QString app_path    = QCoreApplication::applicationDirPath();
    QString fileName    = "config.ini";
    //separator : 分隔符  toNativeSeparators : 分隔符转本地
    QString config_path = QDir::toNativeSeparators(app_path + QDir::separator() + fileName);
    QSettings settings(config_path, QSettings::IniFormat);
    QString gate_host   = settings.value("GateServer/host").toString();
    QString gate_port   = settings.value("GateServer/port").toString();
    gate_url_prefix     = "http://" + gate_host + ":" + gate_port;

    MainWindow w;
    w.show();
    return a.exec();
}
