#ifndef USERDATA_H
#define USERDATA_H
#include <QString>
#include "global.h"
#include <memory>
#include <QVector>
#include <QMap>

class SearchInfo {
public:
    SearchInfo(int uid, QString name, QString nick, QString desc, int sex, QString icon, int relation);
    int _uid;
    QString _name;
    QString _nick;
    QString _desc;
    int _sex;
    QString _icon;
    int _relation;
};

//网络推送时需要使用的类型---------
class AddFriendApply {
public:
    AddFriendApply(int from_uid, QString name, QString desc,QString icon, QString nick, int sex);
    int _from_uid;
    QString _name;
    QString _desc;
    QString _icon;
    QString _nick;
    int     _sex;
};

//qtUI层使用的通用数据结构---------
struct ApplyInfo {
    ApplyInfo(int uid, QString name, QString desc,QString icon, QString nick, int sex, int status);

    ApplyInfo(std::shared_ptr<AddFriendApply> addinfo);//-------------
    void SetIcon(QString head){
        _icon = head;
    }
    int _uid;
    QString _name;
    QString _desc;
    QString _icon;
    QString _nick;
    int _sex;
    int _status;
};

struct FriendInfo{
    FriendInfo(int uid, QString name, QString desc,QString icon, QString nick, int sex);
    void SetIcon(QString head){
        _icon = head;
    }
    int _uid;
    QString _name;
    QString _desc;
    QString _icon;
    QString _nick;
    int _sex;
};

class TextChatData;
struct AuthInfo {
    AuthInfo(int uid, QString name,QString nick, QString icon, int sex):
        _uid(uid), _name(name), _nick(nick), _icon(icon),_sex(sex), _thread_id(0){

    }
    void SetChatDatas(std::vector<std::shared_ptr<TextChatData>> chat_datas){
        _chat_datas = chat_datas;
    }
    int _uid;
    QString _name;
    QString _nick;
    QString _icon;
    int _sex;
    int _thread_id;
    std::vector<std::shared_ptr<TextChatData>> _chat_datas;
};

struct AuthRsp {
    AuthRsp(int peer_uid, QString peer_name,QString peer_nick, QString peer_icon, int peer_sex)
        :_uid(peer_uid),_name(peer_name),_nick(peer_nick),_icon(peer_icon),_sex(peer_sex),_thread_id(0){

    }
    void SetChatDatas(std::vector<std::shared_ptr<TextChatData>> chat_datas){
        _chat_datas = chat_datas;
    }
    int _uid;
    QString _name;
    QString _nick;
    QString _icon;
    int _sex;
    int _thread_id;
    std::vector<std::shared_ptr<TextChatData>> _chat_datas;
};

struct UserInfo {

    UserInfo(int uid, QString name, QString icon): _uid(uid), _name(name), _icon(icon),_nick(_name),_sex(0),_desc(""),_last_msg(""){

    }

    UserInfo(int uid, QString name, QString nick, QString icon, int sex, QString desc="", QString last_msg = ""):
        _uid(uid),_name(name),_nick(nick),_icon(icon),_sex(sex),_desc(desc),_last_msg(last_msg){

    }

    UserInfo(std::shared_ptr<FriendInfo> friendinfo):
        _uid(friendinfo->_uid),_name(friendinfo->_name),_nick(friendinfo->_nick), _icon(friendinfo->_icon),_sex(friendinfo->_sex),_desc(""),_last_msg(""){

    }

    UserInfo(std::shared_ptr<AuthInfo> auth):
        _uid(auth->_uid),_name(auth->_name),_nick(auth->_nick), _icon(auth->_icon),_sex(auth->_sex),_desc(""),_last_msg(""){

    }

    UserInfo(std::shared_ptr<AuthRsp> auth):
        _uid(auth->_uid),_name(auth->_name),_nick(auth->_nick),_icon(auth->_icon),_sex(auth->_sex),_desc(""),_last_msg(""){

    }

    UserInfo(std::shared_ptr<SearchInfo> search_info):
        _uid(search_info->_uid),_name(search_info->_name),_nick(search_info->_nick),_icon(search_info->_icon),
        _sex(search_info->_sex), _desc(search_info->_desc),_last_msg(""){

    }

    int _uid;
    QString _name;
    QString _nick;
    QString _icon;
    int _sex;
    QString _desc;
    QString _last_msg;
};

class UserData
{
public:
    UserData();
};

// ==================== 聊天消息 / 本地会话缓存 ====================
// 说明：参考项目用 thread_id（会话线程）作为会话 key，本项目服务端暂时还没有
//      chat_thread / private_chat 表，所以单聊会话先用"对方的 uid"作为 key。
//      以后服务端补上 thread_id，只需要把 ChatSessionData 的 key 换成 thread_id。

class ChatDataBase{
public:
    ChatDataBase(QString unique_id, int send_uid, ChatMsgType msg_type, QString content, MsgStatus status, QString chat_time = QString());
    ~ChatDataBase();
    QString GetUniqueId()const;
    int GetSendUid()const;
    ChatMsgType GetMsgType()const;
    QString GetContent()const;
    void SetStatus(MsgStatus status);
    MsgStatus GetStatus()const;
    QString GetChatTime()const;
private:
    QString _unique_id;
    int _send_uid;
    ChatMsgType _msg_type;
    QString _content;
    MsgStatus _status;    //这个status是你发送的msg相对于对方的状态——已读，未读... 只有将其设置在ChatItemBase上，ChatItemBase然后再设置图片
                                                                            //，对应的每个气泡组件才会显示对应的状态
    QString _chat_time;
};

class TextChatData : public ChatDataBase {
public:
    TextChatData(QString unique_id, int send_uid, QString content, MsgStatus status = MsgStatus::UN_READ, QString chat_time = QString());
    ~TextChatData();
};

class ChatSessionData{
public:
    ChatSessionData(int other_uid);
    ~ChatSessionData();
    int GetOtherUid()const;
    const QVector<std::shared_ptr<ChatDataBase>>& GetMsgList()const;
    void UpdateMsgStatus(const QString& unique_id, MsgStatus status);
    void AppendMsg(const QString& unique_id, std::shared_ptr<ChatDataBase> msg);   //保持先后顺序
    std::shared_ptr<ChatDataBase> GetMsgByUniqueId(const QString& unique_id)const;   //unique_id -> msg
    QString GetLastMsgContent()const;
    QString GetLastMsgTime()const;
private:
    int _other_uid;
    QVector<std::shared_ptr<ChatDataBase>> _msg_list;
    QMap<QString, std::shared_ptr<ChatDataBase>> _msg_map;
};

#endif // USERDATA_H
