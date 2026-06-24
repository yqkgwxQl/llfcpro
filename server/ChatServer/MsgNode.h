#pragma once
#include <memory>
#include <iostream>

/*  这里_total_len的长度实际上是max_len+1，这里不会有问题吗？

    _data = new char[_total_len + 1]();   // 分配 8 字节，不是 7
    _data[_total_len] = '\0';             // 第 8 字节写 '\0'
    只是为了方便打印调试用的。如果 _data 被当作 C 字符串输出（比如 cout << _data），有 \0 才安全。
    但实际网络发送时只发 _total_len 字节，不包含最后那个 \0。*/

    //这里不管是MsgNode还是RecvNode、SendNode都是类，都是结构体，都是！！结构化数据 ！！，都只是用来进行代码上的辅助，而非真正的传输数据
class MsgNode{
public:
    MsgNode(short max_len);
    short _total_len;
    short _cur_len;
    char* _data;
    void Clear();
};

//结构化数据 id+data
class RecvNode:public MsgNode{
public:
    RecvNode(short max_len,short msg_id);
    short GetMsgId();
private:
    short _msg_id;
};

class SendNode:public MsgNode{
public:
    SendNode(const char* msg,short max_len,short msg_id);
    short GetMsgId();
private:
    short _msg_id;
};
