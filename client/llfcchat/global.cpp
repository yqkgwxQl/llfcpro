#include "global.h"
#include <QString>

QString gate_url_prefix = "";

//std::function<void(QWidget*)> ——> 变量类型   repolish ——> 变量名   不过变量为一个函数 变量类型为函数类型
std::function<void(QWidget*)> repolish = [](QWidget* w){
    //[virtual] void QStyle::unpolish(QWidget *widget)
    w->style()->unpolish(w);// 第一步：卸除旧样式    unpolish() 的作用是清理当前样式在控件上留下的信息
    w->style()->polish(w);// 第二步：重新应用新样式  polish() 的作用是对控件进行“打磨”，执行与样式相关的初始化设置和属性调整。它会重新检查控件当前的状态和属性，并应用对应的 QSS 样式
};

std::function<QString(QString)> xorString = [](QString input){
    /*string.size()和string.length()的区别:
    1.length()：对于字符串而言，“长度”是一个非常自然和直观的概念，提供此函数是为了提高代码的语义可读性1。
    2.size()：为了让 std::string 的行为与 C++ STL 中的其他容器（如 vector、list、map 等）保持统一的接口风格。
    因为所有的 STL 容器都提供 size() 方法来获取元素个数，而只有 string 拥有 length() 方法1。*/
    QString result = input;
    int length = result.length();
    //input[i].unicode() 是一个 16 位的无符号整数 ;  %255不让它太大
    ushort xor_code = length % 255;
    for(int i = 0; i < length; i++){
        //对每个字符进行异或操作
        //注意：这里假设字符都是ASCII，因此直接转换为QChar
    //input[i] 返回的是一个 QChar 对象，它代表字符串中的一个 UTF-16 编码单元。调用 .unicode() 方法的作用，就是提取出该字符在 Unicode 字符集中对应的底层 16 位无符号整数值（即码点）
        result[i] = QChar(static_cast<ushort>(input[i].unicode() ^ xor_code));
    }
    return result;
};