#include "logger.h"
#include <QCoreApplication>
#include <QDir>
#include <QDateTime>
#include <QThread>

// 注意== 此函数不属于成员函数，就是一个简单普通的函数 ; static :只在本cc文件中可看到
static const char* levelTag(QtMsgType type){
    switch(type){
        case QtDebugMsg:return "Debug";
        case QtInfoMsg:return "Info";
        case QtWarningMsg:return "Warnning";
        case QtCriticalMsg:return "Critical";  //问题很严重，但是程序还是可以运行
        case QtFatalMsg:return "Fatal";     //问题特别严重，程序无法运行
    }
    return "?";
}
Logger::Logger()
    :stream_(&file_),logDir_(""),minLevel_(QtDebugMsg),console_(true),installed_(false){
    stream_.setEncoding(QStringConverter::Utf8);
    install();
}

Logger::~Logger(){
    shutdown();
}

void Logger::install(){
    QMutexLocker lock(&mutex_); //这里的锁是守护锁
    if(!installed_){
        qInstallMessageHandler(&Logger::messageHandler);
        installed_ = true;
    }
}

void Logger::shutdown(){
    QMutexLocker lock(&mutex_);
    //如果安装好了，我需要去掉安装的messageHandler
    if(installed_){
        qInstallMessageHandler(nullptr);
        installed_ = false;
    }
    //如果日志文件还在打开，我一定要刷新一下流，并把文件关一下
    if(file_.isOpen()){
        stream_.flush();
        file_.close();
    }
}

void Logger::openFileForToday(){
    if(logDir_.isEmpty()){
        logDir_ = QCoreApplication::applicationDirPath() + "/logs";
        QDir().mkpath(logDir_);
    }
    const QString day = QDateTime::currentDateTime().toString("yyyyMMdd");  //MM:月，mm:分
    if(file_.isOpen() && dayKey_ == day)
        return;
    if(file_.isOpen())
        file_.close();
    dayKey_ = day;
    file_.setFileName(logDir_ + "/llfcchat_" + day + ".log");
    file_.open(QIODevice::WriteOnly | QIODevice::Append);

}
/*struct QMessageLogContext {
    int         version;    // Qt 内部信息版本号，一般不用管
    const char *category;   // 日志分类（默认 "default"） 对应的模块
    const char *file;       // 打出这行日志的源文件（来自 __FILE__）
    int         line;       // 行号（来自 __LINE__）
    const char *function;   // 所在函数（来自 Q_FUNC_INFO）
};
msg : qDebug() << "hello wite";   msg -> hello wite*/
void Logger::handle(QtMsgType type,const QMessageLogContext& ctx,const QString& msg){
    QMutexLocker lock(&mutex_);
    //1<打开日志文件
    if(type < minLevel_)
        return;
    openFileForToday();
    //2<获取日志所需信息
        //1. eg: 程序debug位置在E:\zhongzhuan2\llfcchatpro2\server\ChatServer\CServer.cc  -> src = CServer.cc
    QString src;
    if(ctx.file){
        src = QString::fromUtf8(ctx.file);  // 将const char* 以utf-8的编码格式转化为QString
        int pos = qMax(src.lastIndexOf('/'),src.lastIndexOf('\\')); // '\\' --> 转义 ，表示 '\'
        if(pos >= 0)
            src = src.mid(pos + 1);
    }
    if(src.isEmpty())
        src = QString::fromUtf8(ctx.category ? ctx.category : "default");
        //2.获取 当前时间 + 线程号
    const QString ts = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz");
    // reinterpret_cast<quintptr> 把它当无符号整数读出来，才能做数值格式化；
    const auto tid = reinterpret_cast<quintptr>(QThread::currentThreadId());
    //3<装填信息
    //QStringLiteral(...)：告诉编译器这个字符串字面量在编译期就转成 UTF-16（QString 内部格式），运行时不重复转换
    const QString line = QStringLiteral("%1 [%2] %3:%4 thread=%5 [msg:%6]").arg(ts).arg(levelTag(type)).arg(src).arg(ctx.line).arg(tid,0,16).arg(msg);
    //4《往指定流里输入日志
    if(file_.isOpen()){
        stream_<< line << "\n";
        stream_.flush();
    }
    if(console_){
        fprintf(stderr, "%s\n", line.toLocal8Bit().constData());
        fflush(stderr);
    }
    /*line                       QString（内存里是 UTF-16 的 Unicode 文本）
      │ .toLocal8Bit()
      ▼
    QByteArray                 转成"本地 8 位编码"的字节（Windows 上≈系统代码页，中文系统是 GBK）
      │ .constData()
      ▼
    const char*                取出指向内部字节的 C 字符串指针（以 \0 结尾）*/
}

void Logger::messageHandler(QtMsgType type,const QMessageLogContext& ctx,const QString& msg){
    auto logger = Logger::GetInstance();
    if(logger)
        logger->handle(type,ctx,msg);
}