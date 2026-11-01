#ifndef GLOBAL_H
#define GLOBAL_H
#include <QWidget>
#include <functional>
#include "QStyle"       //样式类
// #include <memory>
// #include <iostream>
#include <QtNetwork/QNetworkReply>

extern std::function<void(QWidget*)> repolish;
extern QString gate_url_prefix;
extern std::function<QString(QString)> xorString;

enum TipErr{
    TIP_SUCCESS = 0,
    TIP_EMAIL_ERR = 1,
    TIP_PWD_ERR = 2,
    TIP_CONFIRM_ERR = 3,
    TIP_PWD_CONFIRM = 4,
    TIP_VARIFY_ERR = 5,
    TIP_USER_ERR = 6
};

enum ClickLbState{
    Normal = 0,
    Selected = 1
};

enum ReqId{
    ID_GET_VERIFY_CODE = 1001,  //获取验证码
    ID_REG_USER = 1002 ,        //注册用户
    ID_RESET_PWD = 1003,        //重置密码
    ID_LOGIN_USER = 1004,       //用户登录
    ID_CHAT_LOGIN = 1005,       //登陆聊天服务器
    ID_CHAT_LOGIN_RSP= 1006,    //登陆聊天服务器回包
    ID_SEARCH_USER_REQ = 1007, //用户搜索请求
    ID_SEARCH_USER_RSP = 1008, //搜索用户回包
    ID_ADD_FRIEND_REQ = 1009,  //添加好友申请
    ID_ADD_FRIEND_RSP = 1010,  //添加好友申请的回复
    ID_NOTIFY_ADD_FRIEND_REQ = 1011,
    // 注意 1009,1010,1011号信息 eg: ClienA发送给ChatServer1 1009, ChatServer1处理后返回 1010给ClientA, 同时ChatServer1通过grpc拿到ChatServer2，发送给ClientB
    ID_AUTH_FRIEND_REQ = 1013,  //认证好友请求
    ID_AUTH_FRIEND_RSP = 1014,  //认证好友回复
    ID_NOTIFY_AUTH_FRIEND_REQ = 1015,   //通知认证好友申请
    ID_TEXT_CHAT_MSG_REQ  = 1017,  //文本聊天信息请求
    ID_TEXT_CHAT_MSG_RSP  = 1018,  //文本聊天信息回复
    ID_NOTIFY_TEXT_CHAT_MSG_REQ = 1019, //通知用户文本聊天信息

    ID_GET_APPLY_LIST_REQ = 1040,	//用户刚登陆时只返回给他10条 别人想要添加他为好友的申请（因为一条信息是有限大小的，先第一条回复里
    //包含10个apply, 后续用户如果在他的客户端里向下翻动它的申请列表，则需要发送此请求来获取后续apply)
    ID_GET_APPLY_LIST_RSP = 1041,
    ID_GET_FRIEND_LIST_REQ = 1042,	//同ID_GET_APPLY_LIST_REQ
    ID_GET_FRIEND_LIST_RSP = 1043,
};

enum Modules{
    REGISTERMOD = 0, //注册模块
    RESETMOD = 1,
    LOGINMOD = 2,
};

enum ErrorCodes{
    SUCCESS = 0,
    ERR_JSON = 1, //json 解析失败
    ERR_NETWORK = 2, //网络错误
};

struct ServerInfo{
    QString Host;
    QString Port;
    QString Token; //身份验证令牌（通常用于连接后的鉴权）
    int Uid;  //用户的唯一标识符(UserID)
};

//聊天界面几种模式
enum ChatUIMode{
    SearchMode, //搜索模式
    ChatMode, //聊天模式
    ContactMode, //联系模式
    SettingsMode, //设置模式
};

//自定义QListWidgetItem的几种类型
enum ListItemType{
    CHAT_USER_ITEM, //聊天用户
    CONTACT_USER_ITEM, //联系人用户
    SEARCH_USER_ITEM, //搜索到的用户
    ADD_USER_TIP_ITEM, //提示添加用户
    INVALID_ITEM,  //不可点击条目
    GROUP_TIP_ITEM, //分组提示条目
    LINE_ITEM,  //分割线
    APPLY_FRIEND_ITEM, //好友申请
};

enum class ChatRole{
    Self,
    Other
};

struct MsgInfo{
    QString msgFlag;//"text,image,file"
    QString content;//表示文件和图像的url,文本信息
    QPixmap pixmap;//文件和图片的缩略图
};


//消息发送/阅读状态（延迟显示在气泡旁边的小图标）
enum class MsgStatus{
    UN_READ = 0,        //已经发出，但未收到服务器回执/ 对方未读
    SEND_FAILED = 1,    //发送失败
    READED =2           //对方已读
};

enum class ChatMsgType{
    TEXT = 0,
    PIC = 1,
    FILE = 2
};
/*
 * 1.维度           enum MsgStatus（无 class）                         enum class ChatMsgType（有 class）
名字作用域	枚举值直接进入外层作用域，UN_READ、READED 全局可见        只能通过 ChatMsgType::TEXT 访问，裸写 TEXT 编译不过
隐式转 int	✅ 可以：int x = UN_READ;                            ❌ 不行：必须 static_cast<int>(ChatMsgType::TEXT)
与其它类型比较	容易误写：if(UN_READ == SomeOtherEnum::X) 有时能编过	 编不过，类型不匹配直接报错

    2.
        qDebug() << ChatMsgType::TEXT;                     // ❌ 没有匹配的 operator<<
        qDebug() << static_cast<int>(ChatMsgType::TEXT);   // ✅
    3.
        MsgStatus s = UN_READ;                 // ✅ 能编（名字泄漏 + 隐式转换）
        int n = MsgStatus::UN_READ;            // ✅ 能编（无 class 时 :: 也能用，只是多余）

        ChatMsgType t = ChatMsgType::TEXT;     // ✅
        ChatMsgType t2 = TEXT;                 // ❌ error: 'TEXT' was not declared
        int n2 = ChatMsgType::TEXT;            // ❌ error: cannot convert 'ChatMsgType' to 'int'
*/

//申请好友标签输入框最低长度 mix_apply_label_ed_len
const int MIN_APPLY_LABEL_ED_LEN = 40;

const QString add_prefix = "添加标签 ";
//------------对于lb_list而言-----------
//文本离容器右边至少留 5px 的呼吸空间，不至于紧贴着边
const std::size_t tip_offset = 5;
//两标签之间的距离
const std::size_t tip_gap = 15;
//-----------对于grid_wid而言-----------
const std::size_t friend_lb_gap = 2;
const std::size_t friend_lb_vertical_gap = 6;

// 用于测试---------------------------
inline std::vector<QString>  strs ={"hello world !",
                             "nice to meet u",
                             "New year，new life",
                             "You have to love yourself",
                             "My love is written in the wind ever since the whole world is you"};

inline std::vector<QString> heads = {
    ":/res/head_1.jpg",
    ":/res/head_2.jpg",
    ":/res/head_3.jpg",
    ":/res/head_4.jpg",
    ":/res/head_5.jpg"
};

inline std::vector<QString> names = {
    "llfc",
    "zack",
    "golang",
    "cpp",
    "java",
    "nodejs",
    "python",
    "rust",
    "veu",
    "qt",
    "cc",
    "json",
    "protobuf",
    "linux",
    "epoll",
    "javaScript",
    "lua",
    "react"
};
#endif // GLOBAL_H
