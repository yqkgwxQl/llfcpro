#include "userdata.h"

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
    _content(content), _status(status), _chat_time(chat_time){

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
    return iter.value()->SetStatus(status);
}

void ChatSessionData::AppendMsg(const QString& unique_id, std::shared_ptr<ChatDataBase> msg){
    auto iter = _msg_map.find(unique_id);
    if(iter != _msg_map.end()){
        qDebug() << "该unique_id(" << unique_id << ")已在_msg_map里添加，这次添加无效";
        return;
    }
    _msg_map.insert(unique_id, msg);
    _msg_list.push_back(msg);
}

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
