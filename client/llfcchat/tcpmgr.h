#ifndef TCPMGR_H
#define TCPMGR_H

#include <QTcpSocket>
#include <functional>
#include <QObject>
#include "singleton.h"
#include "global.h"
#include <QMap>
#include <QByteArray>
#include "userdata.h"

class TcpMgr: public QObject,public Singleton<TcpMgr>,public std::enable_shared_from_this<TcpMgr>{
    Q_OBJECT
    friend class Singleton<TcpMgr>;
public:
    ~TcpMgr();
private:
    TcpMgr();
    QMap<ReqId,std::function<void(ReqId id,int len,QByteArray data)>> _handlers;
    void initHandlers();
    void handleMsg(ReqId reqId,int len,QByteArray data);
    QString _host;
    uint16_t _port;
    QTcpSocket _socket;
    QByteArray _buffer;
    bool _b_recv_pending;
    quint16 _message_id;
    quint16 _message_len;
public slots:
    void slot_tcp_connect(ServerInfo);
    void slot_send_data(ReqId reqId, QByteArray dataBytes);
signals:
    void sig_con_success(bool bsuccess);
    void sig_send_data(ReqId reqId, QByteArray dataBytes);
    void sig_connection_closed();
    void sig_login_failed(int errCode);
    void sig_switch_chatdlg();
    void sig_user_search(std::shared_ptr<SearchInfo>);
    void sig_friend_apply(std::shared_ptr<AddFriendApply>);
    void sig_add_auth_friend(std::shared_ptr<AuthInfo>);
    void sig_auth_rsp(std::shared_ptr<AuthRsp>);
    void sig_applylist_rsp(const QJsonObject& obj);
    void sig_friendlist_rsp(const QJsonObject& obj);
    void sig_receive_text_chat_msg(int from_uid);
};

#endif // TCPMGR_H
