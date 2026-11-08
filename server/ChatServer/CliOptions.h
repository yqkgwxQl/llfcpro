// CliOptions.h
#pragma once

#include <string>
#include <iosfwd>
#include <boost/filesystem.hpp>
#include <iostream>
#include <string>
#include <vector>
#ifdef _WIN32
#  include <windows.h>
#endif

namespace fs = boost::filesystem;

namespace Util{
    //退出码
    //constexpr：编译期常量，比 #define 有类型、有作用域。
                                    // 0 : 正常退出	收到 Ctrl+C 后干净关闭
    constexpr int kExitUsage   = 2; //用法错误	参数拼错、缺参数值
    constexpr int kExitConfig  = 3; //配置错误	文件不存在、缺 [SelfServer]
    constexpr int kExitStartup = 4; //启动失败	端口被占、gRPC 起不来

    struct Options {
        boost::filesystem::path config;   // 空 = 未指定
        boost::filesystem::path logDir;   // 空 = 未指定
        bool help = false;
    };
    //fs::path 本身能表达“空”（path().empty() 为真）

    enum class ParseStatus { Ok, HelpRequested, Error };    //ParseArgs()的返回值类型

    //打印用法
    void PrintUsage(std::ostream& os, const char* prog);
    //取“下一个参数” ——————> 这个函数的详细理解需要结合下面的ParseArgs()函数
    bool TakeValue(int argc, char* argv[], int& i, const std::string& name, std::string& out, std::string& err);

    //------------ParseArgs()函数是核心函数，需要着重关注
    // 解析每一个参数 ————>  只解析、不读盘、不打印；出错时把原因写进 err；同理下面的ResolveConfig()函数也是只把错误原因写进err。不对err进行打印
    ParseStatus ParseArgs(int argc, char* argv[], Options& opt, std::string& err);
    // 补全为绝对路径并校验存在性/类型
    bool ResolveConfig(Options& opt, std::string& err);

    // 取出可执行文件所在目录（与 CWD 无关）
    boost::filesystem::path ExeDir();
    /*  为什么要有这个函数？
        工作目录和 exe 在哪是两个独立概念
            exe 位置：CHATSERVER1.exe 这个文件放在磁盘哪个目录。
            工作目录（CWD，current working directory）：这个进程“站在”哪个目录——从启动它的父进程继承而来，跟 exe 文件在哪毫无关系。

    你敲的命令	                                                进程 CWD	                            config.ini 实际被解析成	    结果
    cd build 后 .\CHATSERVER1.exe	                             ...\build	                        ...\build\config.ini	    ✅ 有（就是 CMake 复制的那份）
    在仓库根敲 .\server\ChatServer\build\CHATSERVER1.exe	llfcchatpro	                            llfcchatpro\config.ini	    ❌ 不存在 → 空配置
    在 C:\Users\22688 敲绝对路径	                        C:\Users\22688	                        C:\Users\22688\config.ini	❌
    双击 exe	                                            ...\build	                            ...\build\config.ini	    ✅
    快捷方式里“起始位置”填了 D:\tmp	                        D:\tmp	                                 D:\tmp\config.ini	         ❌
    注册成 Windows 服务	                                    常是 C:\Windows\System32	             System32\config.ini	    ❌
    Start-Process ... -WorkingDirectory D:\x	            D:\x	                                D:\x\config.ini	            ❌

        同一个 exe，同一份代码，只因为你“站在哪儿”不同，读到的配置就不同。 
        therefore 应该取exe的路径，而非CWD  ——————> 即ExeDir()函数  */

}

