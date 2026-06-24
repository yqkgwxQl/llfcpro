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
    void initHandlers();
    QMap<ReqId,std::function<void(ReqId id,int len,QByteArray data)>> _handlers;
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
    void slot_send_data(ReqId reqId,QString data);
signals:
    void sig_con_success(bool bsuccess);
    void sig_send_data(ReqId reqId,QString data);
    void sig_connection_closed();
    void sig_login_failed(int errCode);
    void sig_switch_chatdlg();
    void sig_user_search(std::shared_ptr<SearchInfo>);
    void sig_friend_apply(std::shared_ptr<AddFriendApply>);
    void sig_add_auth_friend(std::shared_ptr<AuthInfo>);
    void sig_auth_rsp(std::shared_ptr<AuthRsp>);
};

#endif // TCPMGR_H
