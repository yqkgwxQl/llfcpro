#include "httpmgr.h"

HttpMgr::~HttpMgr(){

}

HttpMgr::HttpMgr() : _manager(new QNetworkAccessManager(this)){
    connect(this,&HttpMgr::sig_http_finish,this,&HttpMgr::slot_http_finish);
}

void HttpMgr::PostHttpReq(QUrl url,QJsonObject jsonObj,ReqId req_id,Modules mod){
    // 1< 将json数据序列化---------------
    /*补充1== QJsonObject                QJsonValue（万能值）:QJsonObject({})/QJsonArray([])     QJsonDocument是 QJsonValue + 解析/序列化工具。
    eg: { "key": value, ... }   { {},{} } 、 [ {} 、[] ]                                QJsonValue + fromJson()/toJson()
     补充2==                     QJsonObject                                                 QJsonDocument
    本质          无序的键值容器（类似 QMap<QString,QJsonValue>），只装 {...}           整份 JSON 文档的封装，装一个根 QJsonValue（对象/数组都行）
    对应JSON          一个 object                                                           一份完整文档
    能装数组根吗      不能（它自己就是对象）                                             能（doc.array() 可取出数组根）
    能自己解析/序列化文本吗 不能                                                     fromJson() 解析、toJson() 序列化
    典型用法            组装字段：obj["name"]="alice";                                   包装后变文本 / 从文本还原*/
    QByteArray data = QJsonDocument(jsonObj).toJson();
    //2<设置请求头的两个属性： 内容类型 ，数据长度---------------
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader,"application/json");
    request.setHeader(QNetworkRequest::ContentLengthHeader,QByteArray::number(data.length()));
    //data是序列化的数据，data.length()即为body的长度 假设为37，由于Http按字节流进行传输，数字37must转换为"37",即'3''7'，才能避免大小端问题
    auto self = shared_from_this();
    // reply 为发送post请求后返回的响应 ||| request请求： 请求头 + 空行 + 内容  Qt 的底层库会自动帮你加上这个空行，并把 data 拼接到后面。
    QNetworkReply* reply = _manager->post(request,data);
    //这里服务端返回响应才会发送信号，但是服务端一定会阻塞地发送完验证码之后，然后才给客户端回应请求
    //收到reply后，会发送一个reply::finished信号 触发回调
    //这里self作为函数内部的局部变量，会被销毁， lambda表达式里再传入self,使共享指针引用计数+1,防止包括self在内的所有引用计数都减完之后，变量销毁，执行lambda表达式内部的内容,取this指针（已被释放）的时候出现崩溃
    //3<对于request返回的reply通知信号的处理---------------
    //这个信号是 通知信号完成的
    connect(reply,&QNetworkReply::finished,[self,reply,req_id,mod](){
        //处理错误情况
        if(reply->error() != QNetworkReply::NoError){
            qDebug() << reply->errorString();
            //发送信号通知完成
            emit self->sig_http_finish(req_id,"",ErrorCodes::ERR_NETWORK,mod);
            //等reply不用的时候进行回收
            reply->deleteLater();
            return;
        }
        //无错误
        QString res = reply->readAll();
        //发送信号通知完成
        emit self->sig_http_finish(req_id,res,ErrorCodes::SUCCESS,mod);
        reply->deleteLater();
        return;
    });
}

//根据模块，发送对应的信号给RegisterDialog
void HttpMgr::slot_http_finish(ReqId id,QString res,ErrorCodes err,Modules mod){
    if(mod == Modules::REGISTERMOD){
        //发送信号通知指令模块http响应结束了
        emit sig_reg_mod_finish(id,res,err);
    }
    if(mod == Modules::RESETMOD){
        emit sig_reset_mod_finish(id,res,err);
    }
    if(mod == Modules::LOGINMOD){
        emit sig_login_mod_finish(id,res,err);
    }
}