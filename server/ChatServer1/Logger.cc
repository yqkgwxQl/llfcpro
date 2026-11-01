#include "Logger.h"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <filesystem>
#include <limits>
#include <thread>
#include <utility>

#if defined(_WIN32)
#include <windows.h>   // SetConsoleOutputCP / GetCurrentThreadId
#endif

// ============================================================================
//  lfclog：全局开关（不依赖 Logger 对象，单例被静态析构后依然安全）
// ============================================================================
namespace lfclog {
namespace {
std::atomic<bool> g_active{false};  //全局变量，表示"日志系统当前是否已安装且未 shutdown"。
std::atomic<int>  g_minLevel{static_cast<int>(LogLevel::Info)}; //？？？？？默认info，有点意思
}  // namespace

//load() = 原子地读；store(on) = 原子地写。
//std::memory_order_relaxed 是什么意思？ 内存序回答的是这个问题："这次读写，要不要和别的内存读写排先后？"
    //relaxed（宽松）只保证：这一次读写本身是原子的、真实的。不保证：它和别的内存操作之间的先后顺序
bool active() { return g_active.load(std::memory_order_relaxed); }

void setActive(bool on) { g_active.store(on, std::memory_order_relaxed); }

bool enabled(LogLevel level) {
    return active() && static_cast<int>(level) >= g_minLevel.load(std::memory_order_relaxed);
}

void setMinLevel(LogLevel level) {
    g_minLevel.store(static_cast<int>(level), std::memory_order_relaxed);
}

LogLevel minLevel() { return static_cast<LogLevel>(g_minLevel.load(std::memory_order_relaxed)); }

// 级别名：与客户端 levelTag() 一一对应（客户端原文的 "Warnning"/"Critical" 这里修正为
// Warn/Error，QtMsgType 的五档 = Debug/Info/Warning/Critical/Fatal）
const char* levelTag(LogLevel level) {
    switch (level) {
        case LogLevel::Debug: return "Debug";
        case LogLevel::Info:  return "Info";
        case LogLevel::Warn:  return "Warn";
        case LogLevel::Error: return "Error";
        case LogLevel::Fatal: return "Fatal";
    }
    return "?";
}

bool parseLevel(const std::string& text, LogLevel& out) {
    std::string s = text;
    //transform  :  逐元素就地转大写
    /*参数	                含义
    s.begin(), s.end()	输入区间（要遍历的元素范围，左闭右开）
    s.begin()	        输出起点（结果往哪写）
    lambda	            怎么变换（对每个元素做什么）
    toupper : 就是把小写字母转成大写，不是小写字母就原样返回，不会报错。*/
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
    /*  auto it = std::find_if(first, last, pred); 从 first 到 last 逐个调用 pred()，返回第一个结果为 true 的元素的迭代器
        s.erase(s.begin(), std::find_if(s.begin(), s.end(), not_space));          // 去开头空白
        s.erase(std::find_if(s.rbegin(), s.rend(), not_space).base(), s.end());   // 去结尾空白
        .base()  把正向迭代器等价换为反向迭代器，反之亦然*/
    const auto not_space = [](char c) { return c != ' ' && c != '\t' && c != '\r' && c != '\n'; };
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), not_space));
    s.erase(std::find_if(s.rbegin(), s.rend(), not_space).base(), s.end());
    /*  parseLevel结果：
        "  Debug  "   -> [Debug]
        "\tInfo\r\n"  -> [Info]
        "Warn"        -> [Warn]          （本来就没空白，不动）
        ""            -> []              （空串：第一次 erase 是空操作，第二次 erase(end(),end()) 也是）
        "   "         -> []              （全是空白 → 删空）
        " \t\r\n "    -> []
        "DE BUG"      -> [DE BUG]        ← 中间的空格被保留！*/
    if (s == "DEBUG") { out = LogLevel::Debug; return true; }
    if (s == "INFO")  { out = LogLevel::Info;  return true; }
    if (s == "WARN" || s == "WARNING") { out = LogLevel::Warn; return true; }
    if (s == "ERROR") { out = LogLevel::Error; return true; }
    if (s == "FATAL") { out = LogLevel::Fatal; return true; }
    return false;
}

void logf(LogLevel level, const char* file, int line, const char* fmt, ...) {
    if (!enabled(level)) return;              // 级别不够直接返回，连格式化都不做
    va_list ap;
    va_start(ap, fmt);
    auto logger = Logger::GetInstance();      // 单例可能已被静态析构 → 判空
    if (logger) logger->handleV(level, file, line, fmt, ap);
    va_end(ap);
}

}  // namespace lfclog

// ============================================================================
//  小工具
// ============================================================================
namespace {

    //将时间戳转换成本地时间的分解结构 struct tm  eg:
    /*  #include <iostream>
        #include <ctime>

        int main() {
            time_t t = time(nullptr);
            struct tm tmBuf;

            if (localtime_r(&t, &tmBuf) == nullptr) {
                std::cout << "localtime_r failed\n";
                return -1;
            }

            std::cout << tmBuf.tm_year + 1900 << "-"
                    << tmBuf.tm_mon + 1 << "-"
                    << tmBuf.tm_mday << " "
                    << tmBuf.tm_hour << ":"
                    << tmBuf.tm_min << ":"
                    << tmBuf.tm_sec << "\n";
        }*/
void LocalTime(std::time_t t, std::tm& out) {
#if defined(_WIN32)
    localtime_s(&out, &t);      // MinGW-w64 / MSVC 都是 (tm*, const time_t*) 签名  //Windows
#else
    localtime_r(&t, &out);       // POSIX / Linux / macOS 
#endif
}

// "E:/a/b/CSession.cc" -> "CSession.cc"（对应客户端里 qMax(lastIndexOf('/'), lastIndexOf('\\')) 那段）
std::string ShortName(const char* path) {
    if (path == nullptr) return "unknown";
    const char* base = path;
    for (const char* p = path; *p != '\0'; ++p) {
        if (*p == '/' || *p == '\\') base = p + 1;
    }
    return std::string(base);
}

std::string TodayKey() {
    const std::time_t t = std::time(nullptr);
    std::tm tmv{};
    LocalTime(t, tmv);
    char buf[16];
    std::snprintf(buf, sizeof(buf), "%04d%02d%02d",
                  tmv.tm_year + 1900, tmv.tm_mon + 1, tmv.tm_mday);
    return std::string(buf);
}

// 线程号：Windows 下就是系统 tid（和客户端 QThread::currentThreadId() 一个东西）
unsigned long long CurrentThreadId() {
#if defined(_WIN32)
    return static_cast<unsigned long long>(::GetCurrentThreadId());
#else
    return static_cast<unsigned long long>(
        std::hash<std::thread::id>{}(std::this_thread::get_id()));
#endif
}

}  // namespace

// ============================================================================
//  Logger
// ============================================================================
// ============================================================================
//  单例：让 Logger 永生（这是本文件里最重要的一段，务必看完）
// ============================================================================
std::shared_ptr<Logger> Logger::GetInstance() {
    // 基类版本：首次调用时 new Logger() 并放进 Singleton<Logger>::_instance
    std::shared_ptr<Logger> inst = Singleton<Logger>::GetInstance();

    // ★ 把这份引用扣在一个"永不析构"的堆对象里，引用计数永远不会归零，
    //   于是程序退出时 _instance 析构也不会 delete Logger，~Logger() 不会执行。
    //
    //   为什么必须这样（都是实测出来的，不是理论洁癖）：
    //
    //   1) 递归析构 —— 这是真实的元凶。
    //      ~Logger() -> shutdown() 里为了把残留日志冲出去会调用 emit()，
    //      而 emit() 早期版本走的是 Logger::GetInstance()：
    //      此时 shared_ptr 引用计数已经是 0，这次拷贝会把控制块"复活"，
    //      临时对象析构时计数又回到 0 → 再次 _M_dispose() → 又一次 ~Logger()
    //      → 递归析构 + 对象被 delete 两次 → 堆损坏（0xC0000374 / 0xC0000409）。
    //      插桩日志里能清楚看到两遍 "~Logger / shutdown ENTER"。
    //      修了两处：① LogStreamBuf 改用裸指针 owner_，不再走 GetInstance()；
    //                ② 就是这里的"永生"，从根上保证 ~Logger() 不会在退出期执行。
    //
    //   2) 静态析构的先后顺序是未定义的（跨翻译单元、跨不同机制都不保证）。
    //      本项目的其它管理器（RedisMgr / LogicSystem / MysqlMgr ...）都放在
    //      Singleton<T>::_instance 这个"类静态成员"里，它和 Singleton<Logger>::_instance
    //      谁先析构，标准不作保证。一旦 Logger 先死，别人析构函数里的 LOG_xxx
    //      （会解引用 Logger 的 mutex_ / file_）就是 use-after-free。
    //      Logger 永生之后，这类问题在本日志库这一侧直接不存在了。
    //      （这也正是 lfclog::active() / minLevel() 用命名空间里的 std::atomic 的原因：
    //        平凡析构 → 根本没有"析构"这回事 → 任何时刻读它都安全。）
    //
    //   顺带说明：退出阶段写日志本身是可以的 —— 实测一个命名空间作用域静态对象在
    //   析构函数里 LOG_INFO，退出码 0 且正常落盘。所以"永生"不是因为在退出期
    //   不能碰 iostream，而是为了避开上面这两条。
    //
    //   代价：一个 Logger 对象 + 一个控制块常驻内存（几百字节），进程退出时由系统回收。
    //   建议：仍推荐在 main() 末尾显式 shutdown()，把收尾放在自己可控的时机。
    static std::shared_ptr<Logger>* forever = new std::shared_ptr<Logger>(inst);
    (void)forever; /*这里不会引用计数-1 (void)(*forever)也不会引用计数-1   ——————>   
                            (void)forever和(void)(*forever)只是把里面的值置空，forever是左值
     std::shared_ptr<Logger> Logger::GetInstance() 函数调用后，forever作为静态变量不会立即消失，尽管在函数调用之前我们已经
     (void)forever了，关于*forver这个shared_ptr则是永远不会释放，因此，这个Logger永远不会析构
    即使 forever 真的消失了，Logger 也照样不会析构。 因为真正扣住引用的是 new 出来的那个 shared_ptr 对象（在堆上），
        forever 只是"记住它地址的线索"。线索丢了，那个堆对象照样活着、照样持有引用*/
        /*
        让 Logger 永生：把一份引用扣在堆上的 shared_ptr 里，并且永不 delete 它。
        forever 是静态裸指针，只负责记住地址，本身不持有引用，平凡析构。
        真正扣住引用的是 new 出来的那个 shared_ptr 对象，没有任何地方 delete 它，所以引用永不归还。
        因此 Logger 的引用计数永远大于等于 1，~Logger() 永远不会执行。
        static 保证只 new 一次，否则每次调用 GetInstance() 都会泄漏一个 shared_ptr 对象。
        (void)forever 只是压掉 unused-variable 警告，不影响引用计数。
        */
    return inst;
}

Logger::Logger() {
    // 和客户端一样：构造时就接管。
    // 注意主线程第一次调用 Logger::GetInstance() 是通过 Logger::Init()，
    // 所以那之前写在 std::cout 上的内容不会被记录（Init 要放在 main 最前面）。
    install();
}

Logger::~Logger() { shutdown(); }

void Logger::Init(const std::string& serviceName,
                  LogLevel minLevel,
                  bool console,
                  const std::string& logDir,
                  std::size_t maxFileMB,
                  bool redirectStdStreams) {
    auto logger = GetInstance();     // 首次调用会 new Logger() → install()
    if (!logger) return;

    logger->flushPendingBuffers();   // 切换接管方式前，先把残缺行冲出去（此时不持锁）

    {
        std::lock_guard<std::mutex> lock(logger->mutex_);
        if (!serviceName.empty()) logger->serviceName_ = serviceName;
        if (!logDir.empty())      logger->logDir_      = logDir;
        logger->console_ = console;
        // 加括号绕开 windows.h 的 max 宏
        logger->maxBytes_ = (maxFileMB == 0)
                                ? (std::numeric_limits<std::size_t>::max)()
                                : maxFileMB * 1024ull * 1024ull;
        // 服务名/目录可能变了，下次写日志时重新开文件
        if (logger->file_.is_open()) logger->file_.close();
        logger->dayKey_.clear();
        logger->size_ = 0;

        // 按需要打开或撤掉对 std::cout / std::cerr 的接管
        logger->installLocked(redirectStdStreams);
    }

    lfclog::setMinLevel(minLevel);

#if defined(_WIN32)
    if (console) {
        ::SetConsoleOutputCP(CP_UTF8);   // 让中文日志在 MinGW 控制台不乱码
    }
#endif

    // 头两条日志既有用又能顺手验证系统是通的（第一条会把日志文件建出来）
    LOG_INFO("logger started: service=%s dir=%s level=%s console=%d redirect=%d",
             logger->serviceName_.c_str(), logger->logDir_.c_str(),
             lfclog::levelTag(minLevel), static_cast<int>(console),
             static_cast<int>(redirectStdStreams));
    LOG_INFO("log file: %s", logger->currentFile().c_str());
}

// ---- 对标客户端 install() / shutdown() ------------------------------------
void Logger::install() {
    std::lock_guard<std::mutex> lock(mutex_);
    installLocked(true);
}

/*---std::cout << 42;      // ← std::ostream：负责"格式化"（int → "42"）
       │
       ▼ 每个字符交给 streambuf
   std::streambuf     // ← 负责"搬运"：字符往哪去
       │
       ▼
   真正的落点：控制台 / 文件 / 内存字符串
   ---rdbuf() 的两个重载
        std::streambuf* rdbuf() const;                  // ① 读：当前落点
        std::streambuf* rdbuf(std::streambuf* sb);      // ② 写：换成 sb，**并返回【原来那个】**
        因为我们所有 << 最终都会调用 streambuf 的 sputc / sputn，所以：
            换掉 rdbuf = 换掉所有输出的终点。这就是整套日志系统接管 std::cout 的原理*/
void Logger::installLocked(bool redirect) { //redirect 重定向
    installed_ = true;

    // 记下裸指针：LogStreamBuf 输出时直接用它，不走 GetInstance()（避免退出期递归析构）
    coutBuf_.setOwner(this);
    cerrBuf_.setOwner(this);

    if (redirect && oldCout_ == nullptr) {
        // 换掉 rdbuf 就等于接管了所有 std::cout / std::cerr 的输出
        oldCout_ = std::cout.rdbuf(&coutBuf_);  //oldCout_————> 原先你std::cout.rdbuf(&f) 中的f
        oldCerr_ = std::cerr.rdbuf(&cerrBuf_);
        coutBuf_.setOriginal(oldCout_);
        cerrBuf_.setOriginal(oldCerr_);

        // ★ std::cerr 默认带 unitbuf：每写一个 << 就 flush 一次，
        //   那样 std::cerr << "a" << b; 会被拆成两条日志。这里关掉它，
        //   改由本日志库"按行"输出（每行本来就立即落盘，崩溃一样不丢）。
        std::cerr.unsetf(std::ios::unitbuf);
    } else if (!redirect) {
        restoreStdStreamsLocked();
    }

    lfclog::setActive(true);
}

void Logger::restoreStdStreamsLocked() {
    if (oldCout_ != nullptr) {
        std::cout.rdbuf(oldCout_);
        coutBuf_.setOriginal(nullptr);
        oldCout_ = nullptr;
    }
    if (oldCerr_ != nullptr) {
        std::cerr.rdbuf(oldCerr_);
        std::cerr.setf(std::ios::unitbuf);   // 还原 cerr 默认行为
        cerrBuf_.setOriginal(nullptr);
        oldCerr_ = nullptr;
    }
}

/*加锁主要在void Logger::shutdown() ;void Logger::Init() ;std::string Logger::currentFile() ;
void Logger::handleFrom(LogLevel level, const std::string& src, const std::string& msg);
Logger::handleFrom()里会调void Logger::openFileForToday() ;
死锁流程（单线程自锁，不是两个线程在打架）
    假设某处把 flushPendingBuffers() 写进了已经持锁的代码块(比如一次我正常打印日志要调用handleForm())里：

    线程 T 执行到 std::lock_guard<std::mutex> lock(mutex_); → T 拿到 mutex_，计数/所有权归 T。
    T 继续往下调用 flushPendingBuffers()。
    → coutBuf_.flushPending() → 取出本线程的待写串 → emit(buf)。
    → emit 里发现 owner_ 非空 → 调 owner_->handleFrom(level_, srcTag_, msg)。
    → handleFrom 里再次执行 std::lock_guard<std::mutex> lock(mutex_);。
    T 试图锁一把自己已经持有的非递归 mutex → 未定义行为 → 实际就是挂死在这里。*/
void Logger::flushPendingBuffers() {
    // 不加锁：emit 内部会走到 handleFrom() 去加同一把锁，持锁调用会死锁
    coutBuf_.flushPending();
    cerrBuf_.flushPending();
}

void Logger::shutdown() {
    // 顺序很讲究：
    //   1) 先把各线程没换行的残缺行冲出去（此时总闸还开着，走裸指针 owner_，不碰 GetInstance）
    //   2) 再关总闸：之后宏 / 缓冲区都不再产生日志
    //   3) 最后还原 std::cout / std::cerr 并关闭文件
    flushPendingBuffers();
    lfclog::setActive(false);

    std::lock_guard<std::mutex> lock(mutex_);
    restoreStdStreamsLocked();
    if (file_.is_open()) {
        file_.flush();
        file_.close();
    }
    dayKey_.clear();
    size_ = 0;
    installed_ = false;
}

std::string Logger::currentFile() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return curFile_;
}

// ---- 打开日志文件：logs/<服务名>_<日期>.log，超上限换 _1.log ----
void Logger::openFileForToday() {
    const std::string day = TodayKey();

    // 同一天、同一个文件、还没写满 → 继续用（对应客户端 openFileForToday 的判断）
    if (file_.is_open() && day == dayKey_ && size_ < maxBytes_) return;

    if (file_.is_open()) file_.close();
    size_    = 0;
    dayKey_  = day;

    std::error_code ec;
    std::filesystem::create_directories(logDir_, ec);   // 目录不存在就建（对标 QDir().mkpath）

    for (int idx = 0; idx < 10000; ++idx) {
        std::string name = logDir_ + "/" + serviceName_ + "_" + day;
        if (idx > 0) name += "_" + std::to_string(idx);
        name += ".log";

        std::error_code e2;
        const auto sz = std::filesystem::file_size(name, e2);
        const bool exists = !e2;

        if (!exists || static_cast<std::size_t>(sz) < maxBytes_) {
            // Append：服务重启不覆盖旧日志（对标 QIODevice::Append）
            file_.open(name, std::ios::out | std::ios::app | std::ios::binary);
            if (file_.is_open()) {
                size_    = exists ? static_cast<std::size_t>(sz) : 0;
                curFile_ = name;
            }
            return;
        }
    }
}

// ---- 拼行 + 落盘 + 控制台 ------------------------------------------------
void Logger::writeLineLocked(LogLevel level, const std::string& src, const std::string& msg) {
    openFileForToday();

    using namespace std::chrono;
    const auto now = system_clock::now();
    const auto ms  = duration_cast<milliseconds>(now.time_since_epoch()) % 1000;    //ms时间戳
    const std::time_t tt = system_clock::to_time_t(now);                            // s时间戳
    std::tm tmv{};                                                                      
    LocalTime(tt, tmv);     //将时间戳转换成本地时间的分解结构 struct tm  

    char ts[40];
    std::snprintf(ts, sizeof(ts), "%04d-%02d-%02d %02d:%02d:%02d.%03d",
                  tmv.tm_year + 1900, tmv.tm_mon + 1, tmv.tm_mday,
                  tmv.tm_hour, tmv.tm_min, tmv.tm_sec, static_cast<int>(ms.count()));

    // 行格式和客户端保持一致：
    //   时间 [级别] 出处 thread=线程号 [msg:内容]
    //   客户端：ts [Info] CSession.cc:88 thread=8eb7 [msg:xxx]
    std::string line;
    line.reserve(msg.size() + 96);  //reserve 就是提前把内存占好
    line += ts;
    line += " [";
    line += lfclog::levelTag(level);
    line += "] ";
    line += src.empty() ? std::string("unknown") : src;
    {
        char tid[32];
        std::snprintf(tid, sizeof(tid), " thread=%llx", CurrentThreadId());
        line += tid;
    }
    line += " [msg:";
    line += msg;
    line += "]\n";

    // 1) 落盘（每行 flush，崩溃也不丢 —— 和客户端 stream_.flush() 一致）
    if (file_.is_open()) {
        file_.write(line.data(), static_cast<std::streamsize>(line.size()));
        size_ += line.size();
        file_.flush();
    }

    // 2) 控制台。Warn 及以上走 stderr，其余走 stdout
    //    （客户端统一走 stderr，这里按项目原来 std::cout/std::cerr 的习惯分开）
    if (console_) {
        std::FILE* out = (static_cast<int>(level) >= static_cast<int>(LogLevel::Warn)) ? stderr : stdout;
        std::fwrite(line.data(), 1, line.size(), out);
        std::fflush(out);
    }
}

//已有level, src_file, line, msg ——————  将src_file化简为 short_src_file, eg:"E:/a/b/CSession.cc" -> "CSession.cc"
void Logger::handle(LogLevel level, const char* file, int line, const std::string& msg) {
    if (!lfclog::enabled(level)) return;      // 双保险（宏里已经判过一次）

    std::string src;
    if (file != nullptr) {
        src = ShortName(file);
        src += ":";
        src += std::to_string(line);
    }
    handleFrom(level, src, msg);
}

//已有level, short_src_file, line, msg ——————  向.log文件和控制台写日志
void Logger::handleFrom(LogLevel level, const std::string& src, const std::string& msg) {
    if (!lfclog::enabled(level)) return;

    std::lock_guard<std::mutex> lock(mutex_);
    if (!installed_) return;                  // 已经 shutdown，直接丢弃
    writeLineLocked(level, src, msg);
}

//已有level, src_file, line, 通过vsnprintf拿取msg
void Logger::handleV(LogLevel level, const char* file, int line, const char* fmt, va_list ap) {
    if (!lfclog::enabled(level)) return;
    if (fmt == nullptr) fmt = "";

    char buf[2048];
    const int n = std::vsnprintf(buf, sizeof(buf), fmt, ap);

    if (n <= 0) {
        handle(level, file, line, std::string());
        return;
    }
    // n 是"原本想写的长度"，超过缓冲区说明被截断；vsnprintf 保证结尾有 '\0'
    // 加括号绕开 windows.h 的 min 宏
    const std::size_t len = (std::min<std::size_t>)(static_cast<std::size_t>(n), sizeof(buf) - 1);
    handle(level, file, line, std::string(buf, len));
}

// ============================================================================
//  LogStreamBuf：接管 std::cout / std::cerr
// ============================================================================
 /*让每个线程各自攒自己的那一行，凑到换行时才交出去。这就是 thread_local 的用途
    thread_local	每个线程各有一份 map 这个变量；线程之间互不可见
    static（函数内）	在同一条线程内只有一份，活到该线程结束；不是每次调用都重建
    * map（指针）	变量本身是个指针，初值是 nullptr
    if (map == nullptr) map = new ...	懒分配：本线程第一次真正用到时才创建 map
    return *map;	把指针解引用成对象，按引用返回*/
    
    /* 这个是所有线程所共享的Logger内部的成员——>    Logger内部的coutBuf_,cerrBuf_这两个成员的类型是LogStreamBuf
    也就是说每个线程有不同的map，对应不同的缓冲区。但都有这四个变量
        ------
        void Logger::installLocked(bool redirect) { 
            // 记下裸指针：LogStreamBuf 输出时直接用它，不走 GetInstance()（避免退出期递归析构）
            coutBuf_.setOwner(this);
            cerrBuf_.setOwner(this);
            if (redirect && oldCout_ == nullptr) {
                // 换掉 rdbuf 就等于接管了所有 std::cout / std::cerr 的输出
                //class LogStreamBuf : public std::streambuf { ... }; coutBuf_作为派生类，是可以转换为基类streambuf的，因此std::cout.rdbuf(&coutBuf_)是对的
                oldCout_ = std::cout.rdbuf(&coutBuf_);  //oldCout_————> 原先你std::cout.rdbuf(&f) 中的f
                oldCerr_ = std::cerr.rdbuf(&cerrBuf_);  
                coutBuf_.setOriginal(oldCout_);
                cerrBuf_.setOriginal(oldCerr_);
            }
        ------ 可看出install时, coutBuf_.owner = &logger, cerrBuf_.owner = &logger, oldCout_ = std::cout.rdbuf(&f) 中的f(类型为streambuf),
                oldCerr_ = std::cerr.rdbuf(&f2) 中的f2, coutBuf_.original_ = oldCout_, cerrBuf_.original_ = oldCerr_;
        即线程1{
            //每个线程的不同的部分--------------
            std::unordered_map<const LogStreamBuf*, std::string>* map, 虽然线程结束后map作为指针会析构，但是其指向的map空间却没有任何东西去delete它
            第一次写的时候会在线程内的map里创建LogStreamBuf类型的变量对应的Key,value
            map{coutBuf_ : std::string 缓冲区,
                cerrBuf_ : std::string 缓冲区}
            //每个线程的共有部分--------------
            LogStreamBuf coutBuf_{level_:Info, srcTag_:std::cout, original_:oldCout_, owner_:&logger},
            LogStreamBuf cerrBuf_{level_:Error, srcTag_:std::cerr, original_:oldCerr_, owner_:&logger},
            oldCout_ = std::cout.rdbuf(&f) 中的f,
            oldCerr_ = std::cerr.rdbuf(&f2) 中的f2,
        }
        LogStreamBuf    coutBuf_{LogLevel::Info,  "std::cout"};
        LogStreamBuf    cerrBuf_{LogLevel::Error, "std::cerr"};
        std::streambuf* oldCout_ = nullptr;
        std::streambuf* oldCerr_ = nullptr;*/
std::unordered_map<const LogStreamBuf*, std::string>& LogStreamBuf::tlsBuf() {
    // 只声明"指针"的 thread_local：指针是平凡类型，不会注册 TLS 析构器，
    // 从而绕开 MinGW 下 thread_local 析构顺序导致的堆损坏（详见 Logger.h 注释）
    static thread_local std::unordered_map<const LogStreamBuf*, std::string>* map = nullptr;
    if (map == nullptr) {
        map = new std::unordered_map<const LogStreamBuf*, std::string>();
    }
    return *map;
}

int LogStreamBuf::overflow(int ch) {
    if (!lfclog::active()) {
        // 日志系统没开/已关 → 原样交还给真正的控制台缓冲
        return (original_ != nullptr) ? original_->sputc(static_cast<char>(ch)) : ch;
    }
    if (ch == EOF) return 0;

    std::string& buf = tlsBuf()[this];      //第一次写的时候会在线程内的map里创建LogStreamBuf类型的变量对应的Key,value
    if (ch == '\n') {
        emit(buf);
    } else {
        buf.push_back(static_cast<char>(ch));
        if (buf.size() >= 8192) emit(buf);   // 防止有人写超长行把内存撑爆
    }
    return ch;
}

std::streamsize LogStreamBuf::xsputn(const char* s, std::streamsize n) {
    if (n <= 0) return 0;
    if (!lfclog::active()) {
        return (original_ != nullptr) ? original_->sputn(s, n) : n;
    }

    std::string& buf = tlsBuf()[this];
    for (std::streamsize i = 0; i < n; ++i) {
        if (s[i] == '\n') {
            emit(buf);
        } else {
            buf.push_back(s[i]);
        }
    }
    if (buf.size() >= 8192) emit(buf);
    return n;
}

int LogStreamBuf::sync() {
    // 只有日志系统活着才需要我们自己处理；否则交还给原缓冲
    if (!lfclog::active()) {
        return (original_ != nullptr) ? original_->pubsync() : 0;
    }
    // std::flush / std::endl 触发的 flush：把当前攒的内容打出去
    std::string& buf = tlsBuf()[this];
    emit(buf);
    return 0;
}

void LogStreamBuf::emit(std::string& text) {
    if (text.empty()) return;

    // 裁掉 Windows 风格的 '\r'（如果有）
    while (!text.empty() && (text.back() == '\r' || text.back() == '\n')) text.pop_back();
    if (text.empty()) return;

    std::string msg;
    msg.swap(text);        // 交换而不是拷贝，顺便把缓冲清空复用

    // ★ 用安装时记下的裸指针，而不是 Logger::GetInstance()：
    //   如果在 ~Logger() 的收尾路径上再走 GetInstance()，shared_ptr 引用计数
    //   已为 0，这次拷贝会把控制块"复活"，随后又触发一次 ~Logger() → 递归析构 → 崩。
    if (owner_ != nullptr) {
        owner_->handleFrom(level_, srcTag_, msg);
    }
}

void LogStreamBuf::flushPending() {
    std::string& buf = tlsBuf()[this];
    emit(buf);             // emit 内部判空，这里不用重复判断
}

// ============================================================================
//  LogStream：流式写法（LOGI << ... << ...）
// ============================================================================
LogStream::~LogStream() {
    if (moved_) return;                          // 已被移动构造搬走，不要重复写
    if (!lfclog::enabled(level_)) return;

    std::string msg = oss_.str();
    // 老代码习惯写 << std::endl，这里把多余换行裁掉，保证迁移后不多空行
    while (!msg.empty() && (msg.back() == '\n' || msg.back() == '\r')) msg.pop_back();

    if (auto logger = Logger::GetInstance()) {
        logger->handle(level_, file_, line_, msg);
    }
}
