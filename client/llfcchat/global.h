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
