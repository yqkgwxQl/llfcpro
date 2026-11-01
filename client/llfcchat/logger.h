#ifndef LOGGER_H
#define LOGGER_H

#include "singleton.h"
#include <QMutex>
#include <QFile>
#include <QTextStream>

/*  1==流程：
 *  qDebug() << "xxx";          ─┐
    qInfo()  << "yyy";           │ 无论你在哪、哪个线程、
    qWarning()<< "zzz";          │ 哪行代码调用
某个 Qt 库内部的 qWarning()       ─┘
        │
        ▼  Qt 内部统一汇聚
   消息处理器 (QtMessageHandler)   ← 全项目唯一的"总出口"
        │
        ▼
   （默认行为）写 stderr/调试器*
    2==理解：
    默认的处理器只做一件事：把消息打到控制台/调试输出。它不落文件、不带时间戳、不存档、崩溃就丢。你想给日志加"写文件、时间戳、级别标签、出处行号、
        按天轮转"——本质就是换掉这个总出口
    你只有两条路，而这条路显然更优
    3==方案                                      做法                                                                   缺点
    A：改所有调用点        把每处 qDebug()<<... 改成 MyLogger::log(...)                             你代码里几十处，要全改；以后新代码容易忘；第三方 Qt 库内部的日志你根本改不到
    B：qInstallMessageHandle换总出口   在 main 里装一个自己的处理器，Qt 自动把所有日志送过来     需要自己保证线程安全、别在处理器里递归
    4==总结：
    用 qInstallMessageHandler 是因为它是 Qt 日志的全局总出口：装上自定义处理器后，全项目（含你几十处现有 qDebug、第三方 Qt 模块）
    的日志自动带着"级别 + 出处(file:line) + 分类 + 消息"汇聚到你的代码，让你能统一做文件落盘、时间戳、轮转、过滤——零改动现有调用点*/
class Logger:public Singleton<Logger>{
    friend class Singleton<Logger>;
public:
    ~Logger();

private:
    Logger();
    void install();     //接管qDebug/qInfo/qWarning/...
    void shutdown();    //还原默认的handler并关闭文件
    void openFileForToday();
    void handle(QtMsgType type,const QMessageLogContext& ctx,const QString& msg);
    static void messageHandler(QtMsgType type,const QMessageLogContext& ctx,const QString& msg);
    QMutex mutex_;      //保证多线程不会出现多个线程同时写日志
    QFile file_;        // .log
    QTextStream stream_;   // 绑定.log文件的流
    QString logDir_;    // /logs
    QString dayKey_;    //记录打开的.log文件所对应的日期
    QtMsgType minLevel_;
    bool console_;      //是否控制台打印
    bool installed_;    //是否接管
};

#endif // LOGGER_H
