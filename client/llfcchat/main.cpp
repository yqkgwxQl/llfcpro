#include "mainwindow.h"

#include <QApplication>
#include <QFile>
#include <QDebug>
#include <QLatin1String>
#include <QString>
#include <QDir>
#include <QSettings>
#include "logger.h"

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    Logger::GetInstance();
    QFile qss(":/style/stylesheet.qss");
    if(qss.open(QFile::ReadOnly)){
        qDebug("open success");
        //QSS 样式表的语法和 CSS 非常相似，主要由 ASCII 字符组成（比如 { } : ; color red 等）。Latin-1 编码完美兼容 ASCII，且处理速度极快。
                    //使用 QLatin1StringView 可以避免在转换过程中进行不必要的复杂编码检查或转换，提升了性能。
        QString style = QString::fromLatin1(qss.readAll());
        a.setStyleSheet(style);
        qss.close();
        /*这里为什么要用绝对路径？ ——————  主要是为了确保程序在任何情况下都能稳定、准确地找到配置文件
        规避“工作目录”的不确定性使用绝对路径（即 QCoreApplication::applicationDirPath()）而不是相对路径，主要是为了确保程序在任何情况下都能稳定、准确地找到配置文件。
            规避“工作目录”的不确定性 这是最关键的原因。在 C++/Qt 开发中，程序的 “当前工作目录” 并不总是等于 “程序所在的目录”。
            eg:1.IDE 调试时：在 Qt Creator 或 Visual Studio 中运行程序时，默认的工作目录通常是项目的构建目录（如 build-Desktop-Debug），或者是
            项目源码根目录，而不是生成的 .exe 文件所在的目录。如果使用相对路径 config.ini，程序会去构建目录找文件，导致找不到配置。
            2.快捷方式启动：如果用户创建了桌面快捷方式，并在属性中修改了“起始位置”，相对路径就会失效。
            3.被其他程序调用：如果你的程序是被另一个脚本或程序调用的，工作目录可能是调用者的目录。*/
        // 获取当前应用程序的路径 ——> 绝对路径
        QString app_path = QCoreApplication::applicationDirPath();
        QString fileName = "config.ini";
        //separator : 分隔符  toNativeSeparators : 分隔符转本地
        QString config_path = QDir::toNativeSeparators(app_path + QDir::separator() + fileName);
        QSettings settings(config_path,QSettings::IniFormat);
        QString gate_host = settings.value("GateServer/host").toString();
        QString gate_port = settings.value("GateServer/port").toString();
        gate_url_prefix = "http://" + gate_host + ":" + gate_port;
    }else{
        qDebug("Open failed");
    }

    MainWindow w;
    w.show();
    return a.exec();
}
