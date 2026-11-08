#pragma once

// ============================================================================
//  Logger.h —— 服务端日志系统（C++17 / MinGW / Windows 与 Linux 通用，零第三方依赖）
//
//  ── 和客户端 client/llfcchat/logger.h 的对应关系 ───────────────────────────
//
//   客户端（Qt）                          服务端（纯 C++，本文件）
//   ------------------------------------  ------------------------------------
//   Singleton<Logger> 单例模板            Singleton<Logger>（同一套模板，同款写法）
//   install() 里 qInstallMessageHandler   install() 里重定向 std::cout / std::cerr
//   handle(QtMsgType, QMessageLogContext, handle(LogLevel, file, line, msg)
//          QString msg)
//   openFileForToday()  按天开文件         openFileForToday()  按天 + 按大小开文件
//   QMutex 保护                           std::mutex 保护
//   行格式 ts [级别] file:line thread=x [msg:...]  —— 服务端保持完全一致的格式
//
//   两处刻意的不同（都是实测踩坑后改的，别照抄客户端）：
//     · 客户端在 ~Logger() 里 shutdown()；服务端让 Logger 永生、~Logger() 正常情况下
//       压根不会执行，收尾改在 main() 末尾显式 shutdown()。
//       原因有两条，都在 Logger.cc 的"单例：让 Logger 永生"里：
//         ① 在 ~Logger() 里再调 GetInstance() 会把已归零的 shared_ptr 控制块"复活"，
//            导致递归析构 + 对象被 delete 两次（真实崩过）；
//         ② 跨翻译单元的静态对象析构顺序未定义，Logger 与其它 Singleton 谁先死没有保证。
//     · 服务端额外提供 LOG_xxx / LOGI 宏，可以带 file:line；重定向来的行只能标
//       std::cout / std::cerr（拿不到行号）。
//
//  ── 关键思路（服务端的"全局总出口"） ─────────────────────────────────────
//
//   客户端里 qDebug/qInfo/qWarning 全都会汇聚到 qInstallMessageHandler 装的总出口，
//   所以"装一个 handler"就能让全项目几十处 qDebug 零改动地获得时间戳/级别/落盘。
//
//   服务端没有 Qt，但同样有一个"总出口"：std::cout / std::cerr 的 streambuf。
//   本类在 install() 时把
//        std::cout.rdbuf(&coutBuf_)   // 级别 Info
//        std::cerr.rdbuf(&cerrBuf_)   // 级别 Error
//   换成自己的 LogStreamBuf，于是项目里**现有几百处**
//        std::cout << "xxx" << std::endl;
//        std::cerr << "yyy" << std::endl;
//   一行都不用改，就自动变成
//        2026-10-10 23:04:45.437 [Info] std::cout thread=8eb7 [msg:xxx]
//   并且多线程下整行不会互相穿插（每个线程各有一份行缓冲，见 LogStreamBuf 的注释）。
//
//   需要"精确级别 + 文件:行号"时，再用下面的 LOG_xxx / LOGI 宏（见文件末尾）。
//
//  ── 使用三步（4 个 C++ 服务端各自照做一遍） ──────────────────────────────
//   1. 把 Logger.h / Logger.cc 拷进 GateServer、StatusServer、ChatServer1、ChatServer2
//   2. CMakeLists.txt 的 add_executable(...) 里加一行  Logger.cc
//   3. main() 最前面（要在 ConfigMgr::GetInstance() 之前）调用一次：
//          Logger::Init("chatserver1", LogLevel::Debug, true, "logs");
//      并在 main() 末尾（退出前）调用：
//          Logger::GetInstance()->shutdown();
//
//  日志文件：  logs/<服务名>_<yyyyMMdd>.log      超过上限切成 _1.log / _2.log
// ============================================================================

#include "Singleton.h"

#include <atomic>
#include <cstdarg>
#include <cstddef>
#include <fstream>
#include <memory>
#include <mutex>
#include <sstream>
#include <streambuf>
#include <string>
#include <unordered_map>

// GCC/Clang 下让编译器帮忙检查 printf 格式串（写错了直接编译告警）
#if defined(__GNUC__)
#define LOG_PRINTF_ATTR(fmt_idx, arg_idx) __attribute__((format(printf, fmt_idx, arg_idx))) //？？？？
#else
#define LOG_PRINTF_ATTR(fmt_idx, arg_idx)
#endif

// ---------------------------------------------------------------------------
//  日志级别：数值越大越严重，低于 minLevel 的直接丢弃（连格式化都不做）
//
//  ★ 为什么用 Debug/Info/... 这种驼峰，而不是 DEBUG/INFO/...：
//    windows.h 里有  #define ERROR 0 （还有部分头文件 #define TRACE 0），
//    而 Boost.Asio 在 Windows 上会间接包含 windows.h。
//    一旦枚举写成 LogLevel::ERROR，预处理后会变成 LogLevel::0，整个工程编译报错。
//    （客户端那份 Qt 代码没这个问题，因为 Qt 头文件不与 windows.h 打架。）
//
//  级别名与客户端 levelTag() 一致：Debug / Info / Warn / Error / Fatal
//  （客户端原文写的是 "Warnning" 和 "Critical"，这里修正为 Warn / Error，
//    客户端的 QtMsgType 是 Debug/Info/Warning/Critical/Fatal 五档，正好一一对应）
// ---------------------------------------------------------------------------
enum class LogLevel { Debug = 0, Info = 1, Warn = 2, Error = 3, Fatal = 4 };

class Logger;      // 前置声明
class LogStream;   // 流式写法辅助类（定义在本文件末尾）

// ---------------------------------------------------------------------------
//  lfclog 命名空间：日志系统的全局开关
//
//  为什么把"是否启用 / 当前级别"放在命名空间里的全局变量，而不是 Logger 的成员？
//  因为静态对象析构阶段（比如 RedisMgr、LogicSystem 这些单例的析构函数里）也可能写日志，  
//  而那时 Logger 单例本身可能已经被静态析构了。宏先问全局开关，开关为 false 就直接返回，
//  根本不去解引用可能已销毁的单例对象，避免 use-after-free。
// ---------------------------------------------------------------------------
namespace lfclog {

// 日志系统是否已安装且尚未 shutdown
bool active();
// 内部使用：install() / shutdown() 时开关总闸
void setActive(bool on);
// active() && level >= minLevel
bool enabled(LogLevel level);
void setMinLevel(LogLevel level);
LogLevel minLevel();

// 级别名 <-> 字符串（读 config.ini 的 [Log] Level 用）
const char* levelTag(LogLevel level);
bool parseLevel(const std::string& text, LogLevel& out);

// printf 风格统一入口（LOG_xxx 宏都走它）
void logf(LogLevel level, const char* file, int line, const char* fmt, ...) LOG_PRINTF_ATTR(4, 5);

}  // namespace lfclog

// ============================================================================
//  LogStreamBuf —— 把 std::cout / std::cerr 变成日志"总出口"
//
//  原理：std::ostream 的字符最终都交给它绑定的 std::streambuf，
//        重定向 rdbuf 就等于接管了所有输出。这里按行攒字符，攒到 '\n' 就交给 Logger。
//
//  ★ 为什么用 thread_local 缓冲（关键设计）：
//    老代码是一行拆成多次 << 写入的，例如
//        std::cout << "T" << i << " seq=" << j << " ok" << std::endl;
//    这中间随时可能被其它线程插进来。如果所有线程共用一个缓冲，
//    两个线程的字就会拼到同一行里（"T3 T5 seq=17 seq=18 ok ok"）。
//    改成"每个线程各攒一份"，线程之间永远不共享缓冲，
//    于是哪怕一行由 5 次 << 组成，也只会拼出本线程自己的内容 —— 整行天然原子。
//    （这也是 glog 等日志库处理 stdout 重定向的常用手段）
// ============================================================================
class LogStreamBuf : public std::streambuf {
public:
    LogStreamBuf(LogLevel level, std::string srcTag)
        : level_(level), srcTag_(std::move(srcTag)) {}

    // 日志系统 shutdown 后，把输出还给原来的缓冲（控制台照常可用）
    void setOriginal(std::streambuf* original) { original_ = original; }
    // 安装时记下 Logger 的裸指针（见 .cc 里"为什么不能用 GetInstance()"的说明）
    void setOwner(Logger* owner) { owner_ = owner; }

    // 把本线程还没换行的残留内容冲出去（shutdown 时用）
    void flushPending();
    int sync() override;                                                    // std::endl / std::flush
    //这几个都是重载streambuf基类的-------------------
protected:
    int         overflow(int ch) override;                                  // 单字符写入
    std::streamsize xsputn(const char* s, std::streamsize n) override;      // 整串写入（<< "abc"）

private:
    void emit(std::string& text);   // 把攒好的一行交给 Logger

    // (本缓冲区, 当前线程) 各自的待写内容——线程之间不共享，见上面的说明
   
    //
    // ★ 这里返回的是"裸指针指向的堆对象，且故意不释放"，而不是直接声明
    //   static thread_local std::unordered_map<...>：
    //   MinGW 下带非平凡析构函数的 thread_local 对象，其析构器会在
    //   DLL_THREAD_DETACH 阶段执行，实测会踩到 CRT 拆除顺序问题
    //   （STATUS_HEAP_CORRUPTION 0xC0000374，栈在 ~unordered_map）。
    //   改成"指针 + 懒分配 + 不 delete"就没有 TLS 析构器，问题消失；
    //   代价是每个线程泄漏一份空的 map（每个线程最多 2 个条目），可以忽略。
        /*tls = Thread-Local Storage（线程局部存储），Buf = buffer（缓冲区）。
    合起来就是："线程局部的缓冲区" —— 每个线程各有一份、用来攒"还没凑成一整行的字符"的那块地方。
    static thread_local std::unordered_map<const LogStreamBuf*, std::string> map;   // 理想版
        return map;
    语义完全正确，但在你这条工具链（MinGW）上会崩：带非平凡析构函数的 thread_local 对象，会在线程退出时析构
        （实际发生在 DLL_THREAD_DETACH 阶段），我们实测崩在 ~unordered_map 里（STATUS_HEAP_CORRUPTION 0xC0000374）。*/
    static std::unordered_map<const LogStreamBuf*, std::string>& tlsBuf();

    LogLevel        level_;
    std::string     srcTag_;        // 出处标记："std::cout" / "std::cerr"
    std::streambuf* original_ = nullptr;
    Logger*         owner_    = nullptr;   // 安装时记下的 Logger（裸指针）
};

// ============================================================================
//  Logger
// ============================================================================
class Logger : public Singleton<Logger> {
    friend class Singleton<Logger>;   // 允许 Singleton 里 new Logger()

public:
    // ---- 隐藏基类的 GetInstance()：保证 Logger 对象"永生" -----------------
    //  为什么要这么做，见 Logger.cc 里的详细说明（递归析构 + 静态析构顺序未定义）
    static std::shared_ptr<Logger> GetInstance();

    ~Logger();                        // 必须 public：shared_ptr 的删除器需要访问

    // ---- main() 里第一件事就调用它（要早于 ConfigMgr::GetInstance()）--------
    //  serviceName        服务名，决定日志文件名，如 "gateserver" / "chatserver1"
    //  minLevel           最小输出级别
    //  console            是否同时打印到控制台（Windows 下会自动把控制台切到 UTF-8）
    //  logDir             日志目录，不存在会自动创建
    //  maxFileMB          单个文件上限(MB)，超过换 <服务名>_<日期>_1.log；0 = 不按大小切
    //  redirectStdStreams 是否接管 std::cout / std::cerr（强烈建议 true）
    static void Init(const std::string& serviceName,
                     LogLevel minLevel = LogLevel::Info,
                     bool console = true,
                     const std::string& logDir = "logs",
                     std::size_t maxFileMB = 64,
                     bool redirectStdStreams = true);

    // ---- 安装 / 卸载（对标客户端 install() / shutdown()）-------------------
    void install();
    void shutdown();

    // ---- 核心写入接口（对标客户端 handle()）-------------------------------
    //  src 是"出处"：宏调用传 file:line，重定向来的传 "std::cout" / "std::cerr"
    void handle(LogLevel level, const char* file, int line, const std::string& msg);

    // 直接指定出处标记（LogStreamBuf 用）
    void handleFrom(LogLevel level, const std::string& src, const std::string& msg);

    // printf 版本，给 lfclog::logf 用
    void handleV(LogLevel level, const char* file, int line, const char* fmt, va_list ap);

    // 当前日志文件全路径（启动时打一条，方便找文件）
    std::string currentFile() const;

private:
    Logger();
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

    // 打开今天的日志文件（已打开且未跨天、未超上限就直接返回）
    void openFileForToday();
    // 真正落盘 + 控制台输出，调用者必须已持有 mutex_
    void writeLineLocked(LogLevel level, const std::string& src, const std::string& msg);
    // 安装/卸载对 std::cout、std::cerr 的接管
    void installLocked(bool redirect);
    void restoreStdStreamsLocked();
    // 冲掉各线程残留的残缺行（不加锁：内部会走 handleFrom 去加锁，持锁调用会死锁）
    void flushPendingBuffers();

    mutable std::mutex mutex_;          // 保证多线程不会同时写日志（对标客户端 QMutex）
    std::ofstream      file_;           // .log（对标客户端 QFile + QTextStream）
    std::string        serviceName_ = "server";
    std::string        logDir_    = "logs";
    std::string        dayKey_;         // 当前文件对应的日期 yyyyMMdd
    std::string        curFile_;        // 当前日志文件全路径
    std::size_t        size_      = 0;  // 当前文件已写字节数
    std::size_t        maxBytes_  = 64ull * 1024 * 1024;
    bool               console_   = true;
    bool               installed_ = false;

    // std::cout / std::cerr 重定向用
    LogStreamBuf    coutBuf_{LogLevel::Info,  "std::cout"};
    LogStreamBuf    cerrBuf_{LogLevel::Error, "std::cerr"};
    std::streambuf* oldCout_ = nullptr;     //1.用于取消install时回退到原先版本的std::cout(即std::cout原先应该打印到控制台) 2. 用于没有重定向时，恢复原先版本的std::cout
    std::streambuf* oldCerr_ = nullptr;
};

// ============================================================================
//  LogStream —— 流式写法：LOGI << "uid=" << uid << " login";
//
//  整条语句结束时临时对象析构，才真正写日志，所以一行不会被别的线程从中间插进来。
//  用途：把 std::cout 换成 LOGI 就能顺带指定级别；
//        不加级别接管（redirectStdStreams）时也能用它精确控制级别。
// ============================================================================
class LogStream {
public:
    LogStream(LogLevel level, const char* file, int line)
        : level_(level), file_(file), line_(line) {}

    LogStream(LogStream&& other) noexcept
        : level_(other.level_), file_(other.file_), line_(other.line_),
          oss_(std::move(other.oss_)), moved_(other.moved_) {
        other.moved_ = true;   // 被搬空的临时对象析构时不要再写一条
    }
    LogStream(const LogStream&) = delete;
    LogStream& operator=(const LogStream&) = delete;

    ~LogStream();   // 定义在 Logger.cc

    template <typename T>
    LogStream& operator<<(const T& value) { oss_ << value; return *this; }

    // 让 << std::endl / std::flush 能编译通过
    LogStream& operator<<(std::ostream& (*manip)(std::ostream&)) { oss_ << manip; return *this; }
    // 让 << std::hex / std::dec 能编译通过
    LogStream& operator<<(std::ios_base& (*manip)(std::ios_base&)) { oss_ << manip; return *this; }

private:
    LogLevel           level_;
    const char*        file_;
    int                line_;
    std::ostringstream oss_;
    bool               moved_ = false;
};

// ============================================================================
//  使用宏
// ============================================================================

// ---- printf 风格（推荐新代码用：能拿到 file:line，编译期检查格式串）--------
//      LOG_INFO("user %d login from %s", uid, ip.c_str());
//      LOG_ERROR("mysql error: %s", e.what());
#define LOG_DEBUG(...) ::lfclog::logf(LogLevel::Debug, __FILE__, __LINE__, __VA_ARGS__)
#define LOG_INFO(...)  ::lfclog::logf(LogLevel::Info,  __FILE__, __LINE__, __VA_ARGS__)
#define LOG_WARN(...)  ::lfclog::logf(LogLevel::Warn,  __FILE__, __LINE__, __VA_ARGS__)
#define LOG_ERROR(...) ::lfclog::logf(LogLevel::Error, __FILE__, __LINE__, __VA_ARGS__)
#define LOG_FATAL(...) ::lfclog::logf(LogLevel::Fatal, __FILE__, __LINE__, __VA_ARGS__)

// ---- 流式风格（迁移老代码用：把 std::cout 换成 LOGI，std::cerr 换成 LOGE）--
//      LOGI << "user " << uid << " login";     // 等价于上面那条 printf 写法
//      LOGE << "handle read failed, error is " << ec.what();
#define LOGD ::LogStream(LogLevel::Debug, __FILE__, __LINE__)
#define LOGI ::LogStream(LogLevel::Info,  __FILE__, __LINE__)
#define LOGW ::LogStream(LogLevel::Warn,  __FILE__, __LINE__)
#define LOGE ::LogStream(LogLevel::Error, __FILE__, __LINE__)
#define LOGF ::LogStream(LogLevel::Fatal, __FILE__, __LINE__)
