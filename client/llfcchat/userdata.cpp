#include "userdata.h"
#include "chatlocaldb.h"

SearchInfo::SearchInfo(int uid, QString name,QString nick, QString desc, int sex, QString icon, int relation)
    :_uid(uid), _name(name), _nick(nick), _desc(desc), _sex(sex), _icon(icon), _relation(relation){
}

UserData::UserData() {}

AddFriendApply::AddFriendApply(int from_uid, QString name, QString desc,QString icon, QString nick, int sex)
    :_from_uid(from_uid),_name(name),_desc(desc),_icon(icon),_nick(nick),_sex(sex){

}
ApplyInfo::ApplyInfo(int uid, QString name, QString desc,QString icon, QString nick, int sex, int status)
    :_uid(uid),_name(name),_desc(desc),
    _icon(icon),_nick(nick),_sex(sex),_status(status){

}

ApplyInfo::ApplyInfo(std::shared_ptr<AddFriendApply> addinfo)//-------------
    :_uid(addinfo->_from_uid),_name(addinfo->_name),_desc(addinfo->_desc),
    _icon(addinfo->_icon),_nick(addinfo->_nick),_sex(addinfo->_sex),_status(0){

}

FriendInfo::FriendInfo(int uid, QString name, QString desc,QString icon, QString nick, int sex)
    :_uid(uid), _desc(desc), _icon(icon), _nick(nick), _sex(sex){

}

ChatDataBase::ChatDataBase(QString unique_id, int send_uid, ChatMsgType msg_type, QString content, MsgStatus status, QString chat_time)
    : _unique_id(unique_id), _send_uid(send_uid), _msg_type(msg_type),
    _content(content), _status(status), _chat_time(chat_time), _exist_in_localcache(false){

}

ChatDataBase::~ChatDataBase(){

}

QString ChatDataBase::GetUniqueId()const{
    return _unique_id;
}

int ChatDataBase::GetSendUid()const{
    return _send_uid;
}

ChatMsgType ChatDataBase::GetMsgType()const{
    return _msg_type;
}

QString ChatDataBase::GetContent()const{
    return _content;
}

void ChatDataBase::SetStatus(MsgStatus status){
    _status = status;
}

MsgStatus ChatDataBase::GetStatus()const{
    return _status;
}

QString ChatDataBase::GetChatTime()const{
    return _chat_time;
}

void ChatDataBase::SetExistInLocalCache(bool exist){
    _exist_in_localcache = exist;
}

bool ChatDataBase::GetIfExistInLocalCache()const{
    return _exist_in_localcache;
}

TextChatData::TextChatData(QString unique_id, int send_uid, QString content, MsgStatus status, QString chat_time)
    :ChatDataBase(unique_id, send_uid, ChatMsgType::TEXT, content, status, chat_time){

}

TextChatData::~TextChatData(){

}

ChatSessionData::ChatSessionData(int other_uid)
    :_other_uid(other_uid){

}

ChatSessionData::~ChatSessionData(){

}

int ChatSessionData::GetOtherUid()const{
    return _other_uid;
}

const QVector<std::shared_ptr<ChatDataBase>>& ChatSessionData::GetMsgList()const{
    return _msg_list;
}

void ChatSessionData::UpdateMsgStatus(const QString& unique_id, MsgStatus status){
    auto iter = _msg_map.find(unique_id);
    if(iter == _msg_map.end()){
        qDebug() << "该unique_id(" << unique_id << ")错误，并未在_msg_map里添加";
        return;
    }
    if(iter.value() == nullptr){
        qDebug() << "未能找到该unique_id(" << unique_id << ")对应的std::shared_ptr<ChatDataBase>";
        return;
    }
    ChatLocalDb::GetInstance()->UpdateStatus(unique_id, status);
    iter.value()->SetStatus(status);
    return;
}

//落库和落内存must很严格！！！！！！要不然很容易出现问题
AppendMsgCode ChatSessionData::AppendMsg(const QString& unique_id, std::shared_ptr<ChatDataBase> msg, bool persist){
    if(msg == nullptr || unique_id.isEmpty())
        return AppendMsgCode::PERSIST_FAILED;

    auto iter = _msg_map.find(unique_id);
    if(iter != _msg_map.end()){
        auto existing = iter.value();
        if(existing == nullptr)
            return AppendMsgCode::PERSIST_FAILED;
        if(existing->GetIfExistInLocalCache())
            return AppendMsgCode::ALREADY_EXISTS;

        // 已存在但只在内存：借这次机会补写一次，然后【直接返回】
        if(persist &&
            ChatLocalDb::GetInstance()->SaveMsg(_other_uid, existing) == ChatLocalDb::SaveResult::OK){
            existing->SetExistInLocalCache(true);
            return AppendMsgCode::ALREADY_EXISTS;          // 补写成功 → 可以 ACK
        }
        return AppendMsgCode::ALREADY_EXISTS_MEMORY_ONLY;  // 还是写不进去 → 不 ACK
    }

    bool memory_only = false;
    if(persist){
        if((ChatLocalDb::GetInstance()->SaveMsg(_other_uid, msg)) == ChatLocalDb::SaveResult::OK)
            msg->SetExistInLocalCache(true);
        else
            memory_only = true;
    }else{
        //persist=false 只用于"从库里回读"，本来就在库
        //只允许 LoadSession 用，避免将来有人误用（persist=false 会无条件标记"在库"）
        msg->SetExistInLocalCache(true);
    }
    _msg_map.insert(unique_id, msg);
    _msg_list.push_back(msg);

    //"两个容器（_msg_map 和 _msg_list）必须同增同减"，但代码里没有任何地方保证它, therefore
    Q_ASSERT(_msg_list.size() == _msg_map.size());      // 两个容器必须一一对应
    return memory_only ? AppendMsgCode::ADDED_MEMORY_ONLY : AppendMsgCode::ADDED;
}
/*
                    NOT_OPEN                          WRITE_FAILED
    故障范围        全局：整个会话一条都写不进去      局部：只有这一条失败
    会不会自愈      ❌ 不会。Open() 只在登录时调一次  ✅ 可能。下一条消息也许就成功了
                    失败后本次运行不会再试            （锁释放了、空间腾出来了）
    不入内存的后果  整个 IM 不可用：发的消息不显示    这一条暂时不显示，其它功能完全正常
                    收的消息也看不到，界面永远空白

    入内存的代价    |全都只在内存（本来就全都写不进去） 且全程不 ACK，服务端兜底|


    therefore---------
    路径                      NOT_OPEN          WRITE_FAILED
    接收（1019 / 1045）             |入内存、不 ACK|
    发送（chatpage）	    |入内存、照常发 ————> 不影响正常的功能；远端数据库里的信息绝对正确，以后本地缓存可以从远端数据库进行拉取|

*/

std::shared_ptr<ChatDataBase> ChatSessionData::GetMsgByUniqueId(const QString& unique_id)const{
    auto iter = _msg_map.find(unique_id);
    if(iter == _msg_map.end()){
        qDebug() << "该unique_id(" << unique_id << ")错误，并未在_msg_map里添加";
        return nullptr;
    }
    if(iter.value() == nullptr){
        qDebug() << "未能找到该unique_id(" << unique_id << ")对应的std::shared_ptr<ChatDataBase>";
        return nullptr;
    }
    return _msg_map.value(unique_id);
}

QString ChatSessionData::GetLastMsgContent()const{
    if(_msg_list.isEmpty())
        return QString();
    auto& last = _msg_list.back();
    return last ? last->GetContent() : QString();
}

QString ChatSessionData::GetLastMsgTime()const{
    if(_msg_list.isEmpty())
        return QString();
    auto& last = _msg_list.back();
    return last ? last->GetChatTime() : QString();
}

