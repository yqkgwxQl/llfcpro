#ifndef USERMGR_H
#define USERMGR_H
#include <memory>
#include "singleton.h"
#include <QString>
#include "userdata.h"
#include <QMap>

class UserMgr:public Singleton<UserMgr>,public std::enable_shared_from_this<UserMgr>
{
    friend class Singleton<UserMgr>;
public:
    ~UserMgr();
    void SetToken(QString token);
    QString GetName();
    int GetUid();
    QString GetNick();
    QString GetIcon();
    int GetSex();
    QString GetDesc();
    std::shared_ptr<UserInfo> GetFriendInfoByUid(int friend_uid); //这里我更期望使用FriendInfo
    std::shared_ptr<ChatSessionData> GetChatSessionByUid(int other_uid);
    std::shared_ptr<ChatSessionData> AddToChatSessionDataMap(int other_uid);
    void RemoveToChatSessionDataMap(int other_uid);
    void SetUserInfo(std::shared_ptr<UserInfo> user_info);

    bool AlreadyApply(int uid);
    void AddToApplyList(std::shared_ptr<ApplyInfo> apply_info);
    void AppendApplyList(QJsonArray array);
    // void RemoveOneApply(int applicant_uid);  //————> 即使添加好友成功了，也不应该移除这条apply，应该进行保留
    std::vector<std::shared_ptr<ApplyInfo>> GetApplyList();

    bool AreadyFriend(int uid);
    void AddToFriendMap(std::shared_ptr<AuthInfo> auth_info);
    void AddToFriendMap(std::shared_ptr<AuthRsp> auth_rsp);
    // void AddToFriendMap(std::shared_ptr<FriendInfo> friend_info);
    void AddToFriendMap(std::shared_ptr<UserInfo> friend_user_info);
    void AddToFriendMap(std::shared_ptr<SearchInfo> search_info);
    void AppendFriendList(QJsonArray array);
    void RemoveOneFriend(int friend_uid);
private:
    UserMgr();
    QString _token;
    std::shared_ptr<UserInfo> _user_info;
    std::vector<std::shared_ptr<ApplyInfo>> _apply_list;
    QMap<int, std::shared_ptr<UserInfo>> _friend_map;
    QMap<int, std::shared_ptr<ChatSessionData>> _chat_session_data_map;
};

#endif // USERMGR_H
