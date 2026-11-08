#include "CliOptions.h"

void Util::PrintUsage(std::ostream& os, const char* prog) {
    os << "usage: " << prog << " [--config <ini>] [--logdir <dir>] [-h]\n\n"
       << "  -c, --config <ini>  节点配置；缺省时依次查找 <exe目录>/config.ini、<当前目录>/config.ini\n"
       << "      --logdir <dir>  日志目录；缺省为 <配置文件所在目录>/logs\n"
       << "  -h, --help          显示本帮助\n";
}

bool Util::TakeValue(int argc, char* argv[], int& i, const std::string& name,
               std::string& out, std::string& err) {
    if (i + 1 >= argc || argv[i + 1][0] == '-') {  
        err = name + " 缺少参数值";
        return false;
    }
    out = argv[++i];
    if (out.empty()) { err = name + " 的参数值不能为空"; return false; }
    return true;
}
/*CHATSERVER --config --logdir D:\logs
                       ↑ 不加这个判断，"--logdir" 会被当成配置文件路径*/


Util::ParseStatus Util::ParseArgs(int argc, char* argv[], Util::Options& opt, std::string& err) {
    std::vector<std::string> positional;   //positional 用来装“不以 - 开头的参数”，也就是 ./chatserver1.exe chatserver2.ini这种情况

    for (int i = 1; i < argc; ++i) {    // 从1开始，因为第一个eg:./chatserver.exe不用考虑
        std::string a = argv[i];
        if (a == "-h" || a == "--help") { opt.help = true; return ParseStatus::HelpRequested; }

        // 支持 --config=path
        std::string name = a;
        std::string inlineValue;
        bool hasInline = false;
        if (a.rfind("--", 0) == 0) {    //rfind(str, pos): 从后往前找  str: 找什么 pos: 从后往前找的范围是(起始位置——>就是0, pos)
                                        // 即从后往前找,范围是(0 , 0) 找到了返回对应位置的下标(无论正着找还是倒着找，都返回正着找的下标)
                                        // a.rfind("--", 0) ————> a如果是以--开头返回0  a.rfind("--", 0) == 0 <===> a是否是以--开头的
            auto eq = a.find('=');
            if (eq != std::string::npos) {
                name        = a.substr(0, eq);
                inlineValue = a.substr(eq + 1);
                hasInline   = true;
            }
        }
        /*
            这里的情况举例：
                1.--config=xxx
                    name == --config
                    value == xxx
                2.--logdir=xxx
                    name == --logdir
                    value == xxx
                3.--config=xxx --logdir=yyy  / --config=xxx --logdir yyy
                    判断两次，同理而已
                4.--config xxx 
                    name == --config
                    value == nullptr
                然后 TakeValue(.., value, ..)  ——> value = xxx
                最终 name == --config 
                    value == nullptr
                5.--cofig 
                if 判断时: hasInline == false && TakeValue(...) == false ——> return false && break
            if判断时：过滤掉既没有内联又没有下一项的情况，即情况5
            剩下的情况是：只有内联的 ————> 情况1，2，3
                         只有下一个 ————> 情况4   
                         既有内联又有下一个 ————> 情况3         
        */
        if (name == "--config" || name == "-c" || name == "--logdir") {
            std::string value = inlineValue;
            if (!hasInline && !TakeValue(argc, argv, i, name, value, err)) return ParseStatus::Error;
            if (value.empty()) { err = name + " 的参数值不能为空"; return ParseStatus::Error; }   //  防止--config= 的情况
            if (name == "--logdir") opt.logDir = value;
            else                    opt.config = value;
            continue;
        }

        //对于./chatserver1.exe chatserver2.ini这种情况 
        if (!a.empty() && a[0] == '-') { err = "未知选项: " + a; return ParseStatus::Error; }
        positional.push_back(a);
    }

    // 允许 CHATSERVER chatserver2.ini 这种位置参数写法
    if (opt.config.empty() && positional.size() == 1) {
        opt.config = positional.front();
    } else if (!positional.empty()) {
        std::string all;
        for (const auto& p : positional) all += "\n  " + p;

        if (!opt.config.empty()) {
            err = "多余的参数（已经用 --config 指定过配置文件了）:" + all;
        } else {
            err = "位置参数过多（最多只能给一个 .ini 作为配置文件）:" + all;
        }
        err += "\n用法：CHATSERVER [--config <ini>] [--logdir <dir>] [<ini>]";
        return ParseStatus::Error;
    }
    return ParseStatus::Ok;
}

// 依次尝试：--config → <exe目录>/config.ini → <CWD>/config.ini
bool Util::ResolveConfig(Util::Options& opt, std::string& err) {
    boost::system::error_code ec;

    //如果分析输入可以得到config路径的话
    if (!opt.config.empty()) {
        opt.config = fs::absolute(opt.config, ec).lexically_normal();   //fs::absolute() ——> 相对路径转绝对路径 lexically_normall() ——> 纯字符串层面整理 ..、.、多余的斜杠，不访问磁盘。所以 ..\nodes\x\..\y\config.ini 会变成干净的绝对路径
        if (ec) { err = "无法解析路径: " + opt.config.string(); return false; }
        if (!fs::is_regular_file(opt.config, ec)) {     //fs::is_regular_file ——> 是否是普通文件
            err = "配置文件不存在: " + opt.config.string();
            return false;
        }
        return true;
    }

    //分析输入不可以得到config路径的情况
    const fs::path candidates[] = {
        ExeDir() / "config.ini",
        fs::current_path() / "config.ini",
    };
    for (const auto& c : candidates) {
        if (fs::is_regular_file(c, ec)) { opt.config = c; return true; }
    }

    std::string tried;
    for (const auto& c : candidates) tried += "  " + c.string() + "\n";
    err = "找不到默认 config.ini，请用 --config 指定。已尝试:\n" + tried;
    return false;
}

fs::path Util::ExeDir() {
#ifdef _WIN32
    wchar_t buf[MAX_PATH] = {};
    if (::GetModuleFileNameW(nullptr, buf, MAX_PATH) == 0) return fs::current_path();
    return fs::path(buf).parent_path();
#else
    boost::system::error_code ec;
    fs::path p = fs::read_symlink("/proc/self/exe", ec);
    return ec ? fs::current_path() : p.parent_path();
#endif
}
/* 1.#ifdef _WIN32：GetModuleFileNameW 只有 Windows 有，所以 <windows.h> 必须只在 Windows 下包含——否则在 Linux 上直接编译失败。
   2. Windows：GetModuleFileNameW(nullptr, buf, MAX_PATH) 把当前进程 exe 的完整路径写进 buf，再 parent_path() 去掉文件名。用 W（宽字符）版本是因为 Windows 内部路径就是 UTF-16，宽字符能正确处理中文路径。
    = {} 把缓冲区清零；
    返回 0 表示失败（罕见），此时退回工作目录，至少不崩。
   3.Linux：/proc/self/exe 是个特殊符号链接，指向当前进程的可执行文件；read_symlink 读出目标再取父目录。
*/


