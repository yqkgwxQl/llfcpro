#include "usermgr.h"
#include <QJsonArray>
#include <QJsonObject>

UserMgr::UserMgr() {

}

UserMgr::~UserMgr(){

}

void UserMgr::SetToken(QString token){
    _token = token;
}

QString UserMgr::GetName(){
    return _user_info->_name;
}

int UserMgr::GetUid(){
    return _user_info->_uid;
}

QString UserMgr::GetNick(){
    return _user_info->_nick;
}

QString UserMgr::GetIcon(){
    return _user_info->_icon;
}

int UserMgr::GetSex(){
    return _user_info->_sex;
}

QString UserMgr::GetDesc(){
    return _user_info->_desc;
}

std::shared_ptr<UserInfo> UserMgr::GetFriendInfoByUid(int friend_uid){
    auto iter = _friend_map.find(friend_uid);
    if(iter == _friend_map.end()){
        qDebug() << "friend_uid error! 未找到friend_uid(" << friend_uid << ")对应的用户";
        return nullptr;
    }
    if(iter.value() == nullptr){
        qDebug() << "friend_uid(" << friend_uid << ")对应的_friend_map里的std::shared_ptr<UserInfo>为空";
        return nullptr;
    }
    return iter.value();
}

std::shared_ptr<ChatSessionData> UserMgr::GetChatSessionByUid(int other_uid){
    auto iter = _chat_session_data_map.find(other_uid);
    if(iter == _chat_session_data_map.end()){
        qDebug() << "other_uid error! 未找到other_uid(" << other_uid << ")对应的ChatSessionData, 现在进行添加";
        auto session = AddToChatSessionDataMap(other_uid);
        return session;
    }
    if(iter.value() == nullptr){
        qDebug() << "other_uid(" << other_uid << ")对应的_chat_session_data_map里的std::shared_ptr<ChatSessionData>为空，现在重新添加";
        RemoveToChatSessionDataMap(other_uid);
        auto session = AddToChatSessionDataMap(other_uid);
        return session;
    }
    return iter.value();
}

std::shared_ptr<ChatSessionData> UserMgr::AddToChatSessionDataMap(int other_uid){
    auto iter = _chat_session_data_map.find(other_uid);
    if(iter != _chat_session_data_map.end()){
        qDebug() << "other_uid(" << other_uid << ")对应的ChatSessionData已经添加到Map中，放弃此次添加";
        return nullptr;
    }
    std::shared_ptr<ChatSessionData> chat_session_data = std::make_shared<ChatSessionData>(other_uid);
    _chat_session_data_map.insert(other_uid, chat_session_data);
    qDebug() << "添加other_uid(" << other_uid << ")到_chat_session_data_map里";
    return chat_session_data;
}

void UserMgr::RemoveToChatSessionDataMap(int other_uid){
    auto iter = _chat_session_data_map.find(other_uid);
    if(iter == _chat_session_data_map.end()){
        qDebug() << "other_uid error! 未找到other_uid(" << other_uid << ")对应的ChatSessionData, 无法移除";
        return;
    }
    _chat_session_data_map.remove(other_uid);
    qDebug() << "移除other_uid(" << other_uid << ")对应的_chat_session_data_map里的数据";
}

void UserMgr::SetUserInfo(std::shared_ptr<UserInfo> user_info){
    _user_info = user_info;
}

/*我发现有个很严重的问题就是我的UserMgr::GetInstance()->AreadyFriend(si->_uid)和UserMgr::GetInstance()->AreadyApply()是不能保证准确性的，
 * 因为它们只加载一部分，只有滑到底部才能真正加载完成，才可以保证准确性。这就导致了我代码里使用这两个接口的地方不一定会得出正确的结果。
 * 难道必须客户端初始化时，服务器就要发送给它所有的friend和apply吗？更好的解决方法？
    ———> 根本原则：谁拥有数据，谁做判定
    展示列表 → 分页懒加载（客户端缓存，为了 UI 流畅）
    关系判定（是不是好友 / 我申请过没有）→ 服务端（MySQL friend / friend_apply 是唯一权威）
    这是两个不同的问题，你不该共用一个数据源。

    而且这在产品上也是标准做法：微信/QQ 搜到一个人，结果里直接显示"发消息"还是"添加到通讯录" —— 这个判断从来没在客户端做过，是搜索响应自带的。*/
bool UserMgr::AlreadyApply(int uid){
    for(auto& apply_info : _apply_list){
        if(apply_info->_uid == uid)
            return true;
    }
    return false;
}

void UserMgr::AddToApplyList(std::shared_ptr<ApplyInfo> apply_info){
    _apply_list.push_back(apply_info);
}


void UserMgr::AppendApplyList(QJsonArray array){
    for(const QJsonValue & value : array){
        int uid = value["uid"].toInt();
        QString name = value["name"].toString();
        QString desc = value["desc"].toString();
        QString icon = value["icon"].toString();
        QString nick = value["nick"].toString();
        int sex = value["sex"].toInt();
        int status = value["status"].toInt();
        // _apply_list.push_back(std::shared_ptr<ApplyInfo>(uid,name,desc,icon,nick,sex,status)); ×××× 这里相当于std::shared<T>()的构造函数
        // _apply_list.push_back(std::shared_ptr<ApplyInfo>(new ApplyInfo(uid,name,desc,icon,nick,sex,status))); √√√√
        _apply_list.push_back(std::make_shared<ApplyInfo>(uid,name,desc,icon,nick,sex,status));             //   √√√√
    }
}

// void UserMgr::RemoveOneApply(int applicant_uid){
//     if(!AreadyFriend(applicant_uid)){
//         qDebug() << "无法移除不存在的apply";
//     }else{
//         for(auto iter = _apply_list.begin(); iter != _apply_list.end(); iter++){
//             if((*iter)->_uid == applicant_uid)
//                 _apply_list.erase(iter);
//         }
//     }
//     return;
// }

std::vector<std::shared_ptr<ApplyInfo>> UserMgr::GetApplyList(){
    return _apply_list;
}

bool UserMgr::AreadyFriend(int uid){
    auto filter = _friend_map.find(uid);
    if(filter == _friend_map.end()){
        return false;
    }
    return true;
}

void UserMgr::AddToFriendMap(std::shared_ptr<AuthInfo> auth_info){
    qDebug() << "UserMgr::AddToFriendMap, friend_uid is : " << auth_info->_uid;
    std::shared_ptr<UserInfo> friend_user_info = std::make_shared<UserInfo>(auth_info);
    _friend_map[auth_info->_uid] = friend_user_info;
}

void UserMgr::AddToFriendMap(std::shared_ptr<AuthRsp> auth_rsp){
    qDebug() << "UserMgr::AddToFriendMap, friend_uid is : " << auth_rsp->_uid;
    std::shared_ptr<UserInfo> friend_user_info = std::make_shared<UserInfo>(auth_rsp);
    _friend_map[auth_rsp->_uid] = friend_user_info;
}

// void UserMgr::AddToFriendMap(std::shared_ptr<FriendInfo> friend_info){
//     qDebug() << "UserMgr::AddToFriendMap, friend_uid is : " << friend_info->_uid;
//     std::shared_ptr<UserInfo> friend_user_info = std::make_shared<UserInfo>(friend_info);
//     _friend_map[friend_info->_uid] = friend_user_info;
// }
void UserMgr::AddToFriendMap(std::shared_ptr<UserInfo> friend_user_info){
    qDebug() << "UserMgr::AddToFriendMap, friend_uid is : " << friend_user_info->_uid;
    _friend_map[friend_user_info->_uid] = friend_user_info;
}

void UserMgr::AddToFriendMap(std::shared_ptr<SearchInfo> search_info){
    qDebug() << "UserMgr::AddToFriendMap, friend_uid is : " << search_info->_uid;
    std::shared_ptr<UserInfo> friend_user_info = std::make_shared<UserInfo>(search_info);
    _friend_map[search_info->_uid] = friend_user_info;
}

void UserMgr::AppendFriendList(QJsonArray array){
    for(const auto& element : array){
        const QJsonObject obj_2 = element.toObject();
        int uid = obj_2["uid"].toInt();
        QString name = obj_2["name"].toString();
        QString desc = obj_2["desc"].toString();
        QString icon = obj_2["icon"].toString();
        QString nick = obj_2["nick"].toString();
        int sex = obj_2["sex"].toInt();
        // for(const QJsonValue& value : array)
        // int uid = value["uid"].toInt();
        // QString name = value["name"].toString();
        // QString desc = value["desc"].toString();
        // QString icon = value["icon"].toString();
        // QString nick = value["nick"].toString();
        // int sex = value["sex"].toInt();
        AddToFriendMap(std::make_shared<UserInfo>(uid, name, icon, nick, sex, desc, ""));
    }
}

void UserMgr::RemoveOneFriend(int friend_uid){
    if(!AreadyFriend(friend_uid)){
        qDebug() << "无法移除不存在的friend";
    }else{
        _friend_map.remove(friend_uid);
    }
    return;
}