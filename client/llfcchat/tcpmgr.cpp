#include "tcpmgr.h"
#include <QJsonObject>
#include "usermgr.h"
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonParseError>
#include "userdata.h"
#include "applyfriendpage.h"


namespace {
/* 把网络包体里的 JSON 打印成人能读的多行缩进格式。
   不是合法 JSON（比如二进制包）就原样返回，不会把日志吞掉。
   注意：必须配合 qDebug().noquote()，否则 QString/QByteArray 里的换行又会被转义成 \n */
QString prettyJson(const QByteArray& raw){
    QJsonParseError err{};
    const QJsonDocument doc = QJsonDocument::fromJson(raw, &err);
    if(err.error != QJsonParseError::NoError || doc.isNull())
        return QString::fromUtf8(raw);
    return QString::fromUtf8(doc.toJson(QJsonDocument::Indented));
}
} // namespace

TcpMgr::TcpMgr()
    :_host(""),_port(0),_b_recv_pending(false),_message_id(0),_message_len(0){
    //slot_tcp_connect(ServerInfo si)的_socket.connectToHost(_host,_port);发出的connected信号，表示连接是否成功
    QObject::connect(&_socket,&QTcpSocket::connected,[this](){
        qDebug() << "connect remote server success,server host is: " << _host << " port is: "<< _port;
        emit sig_con_success(true);
    });
    QObject::connect(&_socket,&QTcpSocket::readyRead,[this](){
        // 读取所有数据并追加到缓冲区
        // 当有数据可读时，读取所有数据
        _buffer.append(_socket.readAll());

        /*stream >> _message_id >> _message_len;为什么要这样写？
         QDataStream stream(&_buffer, QIODevice::ReadOnly);为什么要用QDataStream?
        QDataStream 默认使用大端（BigEndian） 字节序，不管你程序跑在什么 CPU 上，读出来的结果都一样。
        而且 >> 运算符知道 quint16 是 2 个字节，自动从流中取 2 字节并转换。
        发和收都用 QDataStream + 相同字节序，两端就能正确解析。 ————> 这里是收，使用QDataStream
        void TcpMgr::slot_send_data(ReqId reqId, QByteArray dataBytes) ————> 这是发，使用的也是QDataStream */
        // QDataStream stream(&_buffer,QIODevice::ReadOnly);
        // stream.setVersion(QDataStream::Qt_6_5);
        /*forever{
            if(!_b_recv_pending) {
                // 缓冲区够 4 字节吗？（2 + 2）
                if (_buffer.size() < sizeof(quint16) * 2) {
                    return;  // 不够，等下次 readyRead
                }

                // 预读取消息 ID 和 消息长度（不移动缓冲区读取位置？）
                stream >> _message_id >> _message_len;

                // 手动移除缓冲区的包头 4 字节
                _buffer = _buffer.mid(sizeof(quint16) * 2);
            }
        }
    这里有一个隐藏的 Bug：
    QDataStream stream(&_buffer, QIODevice::ReadOnly) 关联的是 _buffer，但 stream >> _message_id 读取后，
    又手动 _buffer = _buffer.mid(...) 把缓冲区截断了。然而 stream 内部有自己的读取位置指针，_buffer.mid() 会创建新对象替换 _buffer，
    但 stream 内部的指针还是指向旧的内存。这会导致后续读消息体时可能读到错误的数据。*/
        forever{
            //_b_recv_pending主要应对包头处理完毕，但是包体却未处理完毕的情况
            if(!_b_recv_pending){
                //包头不够
                if(_buffer.size() < 4){
                    return;
                }
                //手动读包头：大端字节序   目前大部分pc都是小端序，tcp规定网络传输要用大端，对于pc机为大端直接返回原样；对于小端pc机，要转大端才可以
                //qFromBigEndian 就是在做    ！！从大端转为主机字节序！！
                _message_id = qFromBigEndian<quint16>(_buffer.data());
                _message_len = qFromBigEndian<quint16>(_buffer.data() + 2);
                _buffer.remove(0,4); //移除包头
                qDebug() << "Message ID:" << _message_id << ", Length:" << _message_len;
            }
            //处理包体
            if(_buffer.size() < _message_len){
                _b_recv_pending = true;
                // continue;
                return;
            }
            _b_recv_pending = false;
            //这里第一个报文的包体已经处理完毕
            //left(n) 从开头切出 n 个字节，不改变原 _buffer：
            QByteArray messageBody = _buffer.left(_message_len);
            //remove是真正的移除，改变原有_buffer;
            _buffer.remove(0,_message_len);
            qDebug().noquote() << "receive body msg is:\n" << prettyJson(messageBody);
            handleMsg(static_cast<ReqId>(_message_id),static_cast<int>(_message_len),messageBody);
        }
    });

    //这一段实际上是异常处理。触发的场景就是TCP 连接层面的各种失败：连接时对方没开服务、地址写错了、网络不通、连不上但也不拒绝、对方主动断开
    QObject::connect(&_socket,&QTcpSocket::errorOccurred,[&](QAbstractSocket::SocketError socketError){
        qDebug() << "Error:" << _socket.errorString();
        switch (socketError) {
        case QTcpSocket::ConnectionRefusedError:
            qDebug() << "Connection Refused!";
            emit sig_con_success(false);
            break;
        case QTcpSocket::RemoteHostClosedError:
            qDebug() << "Remote Host Closed Connection!";
            break;
        case QTcpSocket::HostNotFoundError:
            qDebug() << "Host Not Found!";
            emit sig_con_success(false);
            break;
        case QTcpSocket::SocketTimeoutError:
            qDebug() << "Connection Timeout!";
            emit sig_con_success(false);
            break;
        case QTcpSocket::NetworkError:
            qDebug() << "Network Error!";
            break;
        default:
            qDebug() << "Other Error!";
            break;
        }
    });

    //客户端断开连接的回调
    QObject::connect(&_socket,&QTcpSocket::disconnected,[this](){
        qDebug() << "Disconnected from server.";
        //并且发送通知到界面
        emit sig_connection_closed();
    });
    //连接发送信号用来发送数据
    QObject::connect(this, &TcpMgr::sig_send_data, this, &TcpMgr::slot_send_data);

    initHandlers();
}

/* QString是字符层面的，QByteArrary是字节层面的
        QString  ──── toUtf8() ────────►  QByteArray     ← 网络/文件/JSON 都走这条
          ▲        toLocal8Bit()  (Windows 中文 = GBK)
          │        toLatin1()     (非 ASCII 会变 '?')
          │
          └──── QString::fromUtf8(ba) ────  QByteArray
                QString::fromLocal8Bit(ba)
                QString::fromLatin1(ba)    */
TcpMgr::~TcpMgr(){

}

void TcpMgr::initHandlers(){
    //注册获取登录回包逻辑
    _handlers.insert(ReqId::ID_CHAT_LOGIN_RSP,[this](ReqId reqId,int len,QByteArray data){
        //忽略len参数   ？？？？？？？这里可以优化，既然你不用len,你干脆别传len
        Q_UNUSED(len);
        // qDebug().noquote() << "handle id is " << reqId << " data is:\n" << prettyJson(data);
        //1.翻译：把字节流变成 JSON 文档。
        QJsonDocument jsonDoc = QJsonDocument::fromJson(data);
        // 验伤：确认翻译过程没有出错（数据是完整的）。  jsonDoc.isNull() <===> jsonDoc.isEmpty()
        if(jsonDoc.isNull()){
            qDebug() << "Failed to create QJsonDocument.";
            return;
        }
        //2.定型：将文档视为对象，以便提取具体属性。
        QJsonObject jsonObj = jsonDoc.object();
        // 判断收到的不是本系统的业务回包   ————>  if 不包含错误码
        if(!jsonObj.contains("error")){
            qDebug() << "jsonObj为空或此jsonObj不属于业务逻辑";
            emit sig_login_failed(ErrorCodes::ERR_JSON);
            return;
        }
        /*这里为什么不像jsonDoc.isNull()一样判断，jsonObj.isEmpty()？
            这里直接一步到位，直接jsonObj.contains("error") ————> 本身排除了jsonObj为空的情况*/

        int error = jsonObj["error"].toInt();
        if(error != ErrorCodes::SUCCESS){
            qDebug() << "Login Failed, err is :" << error;
            emit sig_login_failed(error);
            return;
        }

        std::shared_ptr<UserInfo> user_info(std::make_shared<UserInfo>(jsonObj["uid"].toInt(),jsonObj["name"].toString(),
        jsonObj["nick"].toString(),jsonObj["icon"].toString(),jsonObj["sex"].toInt(),jsonObj["desc"].toString()));
        UserMgr::GetInstance()->SetUserInfo(user_info);
        UserMgr::GetInstance()->SetToken(jsonObj["token"].toString());
        qDebug() << "name :" <<jsonObj["name"].toString() << "uid :" << jsonObj["uid"].toInt() << "token :" << jsonObj["token"].toString();
        // UserMgr::GetInstance()->AppendFriendList(jsonObj["apply_list"].toArray());
        emit sig_switch_chatdlg();
    });
    //ID_SEARCH_USER_RSP
    _handlers.insert(ReqId::ID_SEARCH_USER_RSP,[this](ReqId reqId,int len,QByteArray data){
        //忽略len参数
        Q_UNUSED(len);
        // qDebug().noquote() << "handle id is " << reqId << " data is:\n" << prettyJson(data);
        QJsonDocument jsonDoc = QJsonDocument::fromJson(data);
        if(jsonDoc.isNull()){
            qDebug() << "Failed to create QJsonDocument.";
            emit sig_user_search(nullptr);
            return;
        }
        QJsonObject jsonObj = jsonDoc.object();
        if(!jsonObj.contains("error")){
            qDebug() << "jsonObj为空或此jsonObj不属于业务逻辑";
            emit sig_user_search(nullptr);
            return;
        }
        auto err = jsonObj["error"].toInt();
        if(err != ErrorCodes::SUCCESS){
            qDebug() << "search friend error,err is :" << err;
            emit sig_user_search(nullptr);
            return;
        }

        std::shared_ptr<SearchInfo> search_info = std::make_shared<SearchInfo>(jsonObj["uid"].toInt(),jsonObj["name"].toString(),
                                                                               jsonObj["nick"].toString(),jsonObj["desc"].toString(),
                                                                               jsonObj["sex"].toInt(),jsonObj["icon"].toString(),
                                                                               jsonObj["relation"].toInt());

        emit sig_user_search(search_info);
    });
    //ID_ADD_FRIEND_RSP
    _handlers.insert(ReqId::ID_ADD_FRIEND_RSP,[this](ReqId reqId,int len,QByteArray data){
        Q_UNUSED(len);
        // qDebug().noquote() << "handler id is " << reqId << " data is:\n" << prettyJson(data);
        QJsonDocument jsonDoc = QJsonDocument::fromJson(data);
        if(jsonDoc.isNull()){
            qDebug() << "Failed to create JsonDocument";
            return;
        }
        QJsonObject jsonObj = jsonDoc.object();
        if(!jsonObj.contains("error")){
            qDebug() << "jsonObj为空或此jsonObj不属于业务逻辑";
            return;
        }
        auto err = jsonObj["error"].toInt();
        if(err != ErrorCodes::SUCCESS){
            qDebug() << "add_friend_error, error is " << err;
            return;
        }

        qDebug() << " add_friend_req send success, apply_uid is " << jsonObj["applyuid"].toInt() << " to_uid is " << jsonObj["touid"].toInt();

    });
    //ID_NOTIFY_ADD_FRIEND_REQ
    _handlers.insert(ReqId::ID_NOTIFY_ADD_FRIEND_REQ,[this](ReqId reqId,int len,QByteArray data){
        Q_UNUSED(len);
        // qDebug().noquote() << " handler id is " << reqId << " data is:\n" << prettyJson(data);
        QJsonDocument jsonDoc =  QJsonDocument::fromJson(data);
        if(jsonDoc.isNull()){
            qDebug() << "Failed to create JsonDocument";
            return;
        }
        QJsonObject jsonObj = jsonDoc.object();
        if(!jsonObj.contains("error")){
            qDebug() << "jsonObj为空或此jsonObj不属于业务逻辑";
            return;
        }
        auto err = jsonObj["error"].toInt();
        if(err != ErrorCodes::SUCCESS){
            qDebug() << "notify_add_friend error, error is " << err;
            return;
        }

        auto addFriendApply = std::make_shared<AddFriendApply>(jsonObj["applyuid"].toInt(),jsonObj["name"].toString(),jsonObj["desc"].toString(),
                                                               jsonObj["icon"].toString(),jsonObj["nick"].toString(),jsonObj["sex"].toInt());
        emit sig_friend_apply(addFriendApply);
    });
    _handlers.insert(ReqId::ID_AUTH_FRIEND_RSP,[this](ReqId reqId,int len,QByteArray data){
        Q_UNUSED(len);
        // qDebug().noquote() << "handler id is " << reqId << " data is:\n" << prettyJson(data);
        QJsonDocument jsonDoc = QJsonDocument::fromJson(data);
        if(jsonDoc.isNull()){
            qDebug() << "Failed to create JsonDocument";
            return;
        }
        QJsonObject jsonObj = jsonDoc.object();
        if(!jsonObj.contains("error")){
            qDebug() << "jsonObj为空或此jsonObj不属于业务逻辑";
            return;
        }
        int err = jsonObj["error"].toInt();
        if(err){
            qDebug() << "authen_friend error, error is " << err;
            return;
        }

        std::shared_ptr<AuthRsp> authRsp = std::make_shared<AuthRsp>(jsonObj["touid"].toInt(),jsonObj["name"].toString(),jsonObj["nick"].toString(),
                                                                     jsonObj["icon"].toString(),jsonObj["sex"].toInt());
        qDebug() << "mine Auth Friend Success " ;
        emit sig_auth_rsp(authRsp);
    });
    _handlers.insert(ReqId::ID_NOTIFY_AUTH_FRIEND_REQ,[this](ReqId reqId,int len,QByteArray data){
        Q_UNUSED(len);
        // qDebug().noquote() << "handler id is " << reqId << " data is:\n" << prettyJson(data);
        QJsonDocument jsonDoc = QJsonDocument::fromJson(data);
        if(jsonDoc.isNull()){
            qDebug() << "Failed to create JsonDocument";
            return;
        }
        QJsonObject jsonObj = jsonDoc.object();
        if(!jsonObj.contains("error")){
            qDebug() << "jsonObj为空或此jsonObj不属于业务逻辑";
            return;
        }
        int err = jsonObj["error"].toInt();
        if(err){
            qDebug() << "notify_authen_friend error, error is " << err;
            return;
        }

        std::shared_ptr<AuthInfo> authInfo = std::make_shared<AuthInfo>(jsonObj["peer_uid"].toInt(),jsonObj["peer_name"].toString(),
                                                                        jsonObj["peer_nick"].toString(),jsonObj["peer_icon"].toString(),
                                                                        jsonObj["peer_sex"].toInt());
        qDebug() << "receive peer_notify:peer has auth mine add request";
        emit sig_add_auth_friend(authInfo);
    });
    //ID_GET_APPLY_LIST_RSP
    _handlers.insert(ReqId::ID_GET_APPLY_LIST_RSP,[this](ReqId reqId,int len,QByteArray data){
        //忽略len参数
        Q_UNUSED(len);
        // qDebug().noquote() << "handle id is " << reqId << " data is:\n" << prettyJson(data);
        QJsonDocument jsonDoc = QJsonDocument::fromJson(data);
        if(jsonDoc.isNull()){
            qDebug() << "Failed to create QJsonDocument.";
            // return;      这三个return注释掉是因为必须最后发送信号
        }
        QJsonObject jsonObj = jsonDoc.object();
        if(!jsonObj.contains("error")){
            qDebug() << "jsonObj为空或此jsonObj不属于业务逻辑";
            // return;
        }
        auto err = jsonObj["error"].toInt();
        if(err != ErrorCodes::SUCCESS){
            qDebug() << "get_apply_list error,err is :" << err;
            // return;
        }

        //除了添加applylist到applyfriendpage里，还需要加入客户端自己的UserMgr里
        UserMgr::GetInstance()->AppendApplyList(jsonObj["apply_list"].toArray());
        emit sig_applylist_rsp(jsonObj);
    });
    //ID_GET_FRIEND_LIST_RSP
    _handlers.insert(ReqId::ID_GET_FRIEND_LIST_RSP,[this](ReqId reqId,int len,QByteArray data){
        //忽略len参数
        Q_UNUSED(len);
        // qDebug().noquote() << "handle id is " << reqId << " data is:\n" << prettyJson(data);
        QJsonDocument jsonDoc = QJsonDocument::fromJson(data);
        if(jsonDoc.isNull()){
            qDebug() << "Failed to create QJsonDocument.";
            // return;      这三个return注释掉是因为必须最后发送信号
        }
        QJsonObject jsonObj = jsonDoc.object();
        if(!jsonObj.contains("error")){
            qDebug() << "jsonObj为空或此jsonObj不属于业务逻辑";
            // return;
        }
        auto err = jsonObj["error"].toInt();
        if(err != ErrorCodes::SUCCESS){
            qDebug() << "get_friend_list error,err is :" << err;
            // return;
        }

        //除了添加friendlist到ChatUserList里，还需要加入客户端自己的UserMgr里
        UserMgr::GetInstance()->AppendFriendList(jsonObj["friend_list"].toArray());
        //通知chatdialog初始化自己的chat_friend列表
        emit sig_friendlist_rsp(jsonObj);
    });
    //ID_TEXT_CHAT_MSG_REQ
    _handlers.insert(ReqId::ID_TEXT_CHAT_MSG_RSP,[this](ReqId reqId,int len,QByteArray data){
        //忽略len参数
        Q_UNUSED(len);
        // qDebug().noquote() << "handle id is " << reqId << " data is:\n" << prettyJson(data);
        QJsonDocument jsonDoc = QJsonDocument::fromJson(data);
        if(jsonDoc.isNull()){
            qDebug() << "Failed to create QJsonDocument.";
            // return;      这三个return注释掉是因为必须最后发送信号
        }
        QJsonObject jsonObj = jsonDoc.object();
        if(!jsonObj.contains("error")){
            qDebug() << "jsonObj为空或此jsonObj不属于业务逻辑";
            // return;
        }
        auto err = jsonObj["error"].toInt();
        if(err != ErrorCodes::SUCCESS){
            qDebug() << "receive text_chat_msg_rsp error,err is :" << err;
            // return;
        }

        qDebug() << "receive text_chat_msg_rsp(from_uid: " << jsonObj["from_uid"].toInt() << " to_uid: " << jsonObj["to_uid"].toInt()
                 << ") Success";
    });
    //ID_NOTIFY_TEXT_CHAT_MSG_REQ
    _handlers.insert(ReqId::ID_NOTIFY_TEXT_CHAT_MSG_REQ,[this](ReqId reqId,int len,QByteArray data){
        //忽略len参数
        Q_UNUSED(len);
        // qDebug().noquote() << "handle id is " << reqId << " data is:\n" << prettyJson(data);
        QJsonDocument jsonDoc = QJsonDocument::fromJson(data);
        if(jsonDoc.isNull()){
            qDebug() << "Failed to create QJsonDocument.";
            // return;      这三个return注释掉是因为必须最后发送信号
        }
        QJsonObject jsonObj = jsonDoc.object();
        if(!jsonObj.contains("error")){
            qDebug() << "jsonObj为空或此jsonObj不属于业务逻辑";
            // return;
        }
        auto err = jsonObj["error"].toInt();
        if(err != ErrorCodes::SUCCESS){
            qDebug() << "receive notify_text_chat_msg_req error,err is :" << err;
            // return;
        }

        qDebug() << "receive notify_text_chat_msg_req Success";
        int from_uid = jsonObj["from_uid"].toInt();
        auto chat_session_data = UserMgr::GetInstance()->GetChatSessionByUid(from_uid);
        if(!chat_session_data)
            return;
        for(const auto one_msg_doc : jsonObj["text_array"].toArray()){
            QJsonObject msg_obj = one_msg_doc.toObject();
            QString unique_id = msg_obj["unique_id"].toString();
            QString content = msg_obj["content"].toString();
            MsgStatus status = static_cast<MsgStatus>(msg_obj["status"].toInt());
            QString chat_time = msg_obj["chat_time"].toString();
            chat_session_data->AppendMsg(unique_id, std::make_shared<ChatDataBase>(unique_id, from_uid, ChatMsgType::TEXT, content, status, chat_time));
        }
        emit sig_receive_text_chat_msg(from_uid);
    });
}

void TcpMgr::handleMsg(ReqId reqId,int len,QByteArray data){
    auto find_iter = _handlers.find(reqId);
    if(find_iter == _handlers.end()){
        qDebug()<< "not found id ["<< reqId << "] to handle";
        return;
    }
    find_iter.value()(reqId,len,data);
}

//这里才是真正连接到chatServer
void TcpMgr::slot_tcp_connect(ServerInfo si){
    qDebug()<< "receive tcp connect signal";
    // 尝试连接到服务器
    qDebug() << "Connecting to server...";
    _host = si.Host;
    //toUInt() 将其转为无符号整数
    _port = static_cast<uint16_t>(si.Port.toUInt());
    /*核心操作：调用 QTcpSocket 的 connectToHost 方法向目标服务器发起 TCP 三次握手。
    非阻塞特性：这是 Qt 网络编程的重点。connectToHost 是异步非阻塞的，它只会触发连接动作然后立即返回，不会卡死当前的 UI 或业务线程。
    后续处理：连接的结果（成功或失败）不会在这个函数里体现，而是需要通过监听 _socket 发出的信号来处理：
    如果连接成功，会触发 connected 信号。
    如果发生错误（如拒绝连接、超时），会触发 error 信号2。*/
    _socket.connectToHost(_host,_port);
}


//---------------version 2(slot_send_data)-----------------------------
void TcpMgr::slot_send_data(ReqId reqId, QByteArray dataBytes){
    const quint16 id = static_cast<quint16>(reqId);
    //quint16 len = static_cast<quint16>(dataBytes.length());
    //长度校验：防止quint16回绕(即上面的这行代码 dataBytes.length()可能 > 65536) ; 同时防止服务端 2048断开
    const long long raw = dataBytes.length();
    if(raw <= 0){
        qWarning() << "[TX] 空包，丢弃, id=" << id;
        return;
    }
    if(raw > 65535){
        qWarning() << "[TX] 包体超过 65535(quint16 上限)，丢弃, id=" << id << " len=" << raw;
        return;
    }
    if(raw > 2048){                       // 和服务端 MAX_LENGTH 保持一致
        qWarning() << "[TX] 包体超过服务端上限 2048，发送后会被断开, id=" << id << " len=" << raw;
        // 想更稳的话这里直接 return，别把连接弄死
    }
    const quint16 len = static_cast<quint16>(raw);
    //发送日志
    qDebug().noquote()
        << QString("[TX] id=%1 len=%2 body=%3")
               .arg(id).arg(len).arg(prettyJson(dataBytes));

    QByteArray block;
    //QDataStream 会自动转大端的
    QDataStream out(&block,QIODevice::WriteOnly);
    out.setByteOrder(QDataStream::BigEndian);
    out << id << len;
    block.append(dataBytes);
    // _socket.write(block);

    // 检查写入结果
    const qint64 n = _socket.write(block);
    if(n != static_cast<qint64>(block.size())){
        qWarning() << "[TX] write 不完整, 期望" << block.size()
            << "实际" << n << " socketState=" << _socket.state();
    }
}

//---------------version 1(slot_send_data)----------------------------- 由于传过来的是String，所以会有英文和中文的实际所占字节的不同，要进行utf-8的转换
// void TcpMgr::slot_send_data(ReqId reqId, QString data){
//     quint16 id = static_cast<quint16>(reqId);
//     /*将字符串转换为UTF-8编码的字节数组 ————> 防止发送中文出现问题
//         内容      data.length()       data.toUtf8().length()
//         "hello"       5                     5 ✅
//         "你好"        2                     6 ❌         */
//     QByteArray dataBytes = data.toUtf8();
//     quint16 len = static_cast<quint16>(dataBytes.length());
//     QByteArray block;
//     //QDataStream 会自动转大端的
//     QDataStream out(&block,QIODevice::WriteOnly);
//     out.setByteOrder(QDataStream::BigEndian);
//     out << id << len;
//     //当你使用 << 操作符向 QDataStream 写入 QByteArray 或 QString 等变长类型时，QDataStream 会自动在数据前面加上一个 4 字节的无符号整数（quint32）来表示数据的长度
//     // out << data;
//     //QString 提供的 .toUtf8() 方法会将 Unicode 字符串转换为 UTF-8 编码格式的 QByteArray
//     // block.append(data.toUtf8());
//     block.append(dataBytes);
//     //这里write的时候为什么不像async_write一样用队列保证异步的有序性？
//     /*Qt 的 write() 和 Boost.Asio 的 async_write() 本质不同
//     Qt 的 QTcpSocket::write()
//         // Qt 的做法：write 只是把数据追加到内部缓冲区，立即返回
//         _socket.write(data1);   // 追加到内部缓冲，返回
//         _socket.write(data2);   // 追加到内部缓冲，返回
//         _socket.write(data3);   // 追加到内部缓冲，返回
//                                 // Qt 事件循环逐个从缓冲取出发送
//         Qt 内部维护了一个 QByteArray 写缓冲区：
//         你调用 write(data1) → Qt 内部: [data1]
//         你调用 write(data2) → Qt 内部: [data1][data2]
//         你调用 write(data3) → Qt 内部: [data1][data2][data3]
//                                       ↓ 事件循环异步发送
//                                       发完 data1 → 发 data2 → 发 data3 ✅ 有序
//         Qt 已经帮你排队了。 所以你在客户端 tcpmgr.cpp 中不需要手写发送队列，直接多次调用 write() 就是安全的：
//     -- 客户端 tcpmgr，这样写完全没问题
//         _socket.write(block1);
//         _socket.write(block2);
//         _socket.write(block3);
//         // 对端会按 block1 → block2 → block3 的顺序收到 ✅
//     Boost.Asio 的 async_write()
//     -- Boost.Asio 的做法：每个 async_write 立即开始发送
//         async_write(socket, buffer1, callback1);
//         async_write(socket, buffer2, callback2);  // ❌ 和 buffer1 同时发
//         async_write(socket, buffer3, callback3);  // ❌ 三个并发！数据会交错！
//         Boost.Asio 的 async_write 没有内部排队机制，它会立即操作 socket。所以你必须自己维护一个发送队列，保证同一时刻只有一个 async_write
//          在运行——这就是你 CSession 里 _send_que 的作用。

//     */
//     _socket.write(block);
// }