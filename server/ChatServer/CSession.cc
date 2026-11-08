#include "CSession.h"
#include "Logger.h"
#include "CServer.h"
#include "MsgNode.h"
#include <boost/uuid/uuid_io.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <memory>
#include <iostream>
#include "LogicSystem.h"

CSession::CSession(boost::asio::io_context& ioc,std::shared_ptr<CServer> cserver)
    :_user_uid(0), _socket(ioc), _server(cserver){
    boost::uuids::uuid uuid = boost::uuids::random_generator()();
    std::string uuid_string = boost::uuids::to_string(uuid);
    _session_id = uuid_string;
    memset(_data,0,MAX_LENGTH);
    _recv_head_node = std::make_shared<MsgNode>(HEAD_TOTAL_LEN);
}

CSession::~CSession(){
	LOGI << "~CSession destruct" << std::endl;
}
    
tcp::socket& CSession::GetSocket(){
    return _socket;
}

std::string& CSession::GetSessionId(){
    return _session_id;
}

void CSession::SetUserId(int uid){
	_user_uid = uid;
}

std::size_t CSession::GetUserId(){
	return _user_uid;
}

std::shared_ptr<CServer> CSession::GetServer(){
    return _server;
}
void CSession::Start(){
    AsyncReadHead();
}

void CSession::AsyncReadHead(){
    auto self = shared_from_this();
    AsyncReadFull(HEAD_TOTAL_LEN,[this,self](const boost::system::error_code& ec, std::size_t bytesTransfered){
        try{
            if (ec) {
				LOGI << "handle read failed, error is " << ec.what() << std::endl;
                //这里实际上只传self也是可以的
				// self->Close();
                // self->_server->ClearSessions(self->GetSessionId());
                Close();
                _server->ClearSession(_session_id);
				// DealExceptionSession();
				return;
			}
            if(bytesTransfered < HEAD_TOTAL_LEN){
                LOGI << "read length not match, read [" << bytesTransfered << "] , total ["<< HEAD_TOTAL_LEN << "]" << std::endl;
                Close();
                _server->ClearSession(_session_id);
                return;
            }
            //进行id和长度的解析
            _recv_head_node->Clear();
            memcpy(_recv_head_node->_data,_data,HEAD_TOTAL_LEN);
            short msg_id = 0;
            memcpy(&msg_id,_recv_head_node->_data,HEAD_ID_LEN);
            msg_id = boost::asio::detail::socket_ops::network_to_host_short(msg_id);
            short msg_len = 0;
            memcpy(&msg_len,_recv_head_node->_data + HEAD_ID_LEN,HEAD_DATA_LEN);
            msg_len = boost::asio::detail::socket_ops::network_to_host_short(msg_len);

            LOGI << "msg_id is " << msg_id << std::endl;
			//id非法
			if (msg_id > MAX_LENGTH) {
				LOGI << "invalid msg_id is " << msg_id << std::endl;
                Close();
				_server->ClearSession(_session_id);
				return;
			}
            LOGI << "msg_len is " << msg_len << std::endl;
			//len非法
			if (msg_len > MAX_LENGTH) {
				LOGI << "invalid msg_len is " << msg_len << std::endl;
                Close();
				_server->ClearSession(_session_id);
				return;
			}
            
            _recv_msg_node = std::make_shared<RecvNode>(msg_len,msg_id);
            AsyncReadBody(msg_len);

        }catch(std::exception& e){
            LOGI << "Exception code is " << e.what() << std::endl;
        }
    });
}
/* 客户端连接上来
    ↓
CServer::handle_accept → 创建 shared_ptr<CSession>
    ↓
session->Start() → AsyncReadHead → AsyncReadFull → async_read_some
    ↓                                                      ↑
  函数返回，CServer 的 _sessions map 中                            │
  还持有这个 session 的 shared_ptr                                │
                                                              │
如果某处从 _sessions 移除了这个 session：
    _sessions.erase(session_id)                                  │
    ↓                                                            │
    shared_ptr 引用计数减 1                                       │
    ↓                                                            │
    如果引用计数归零 → session 被析构 → _socket 被销毁              │
    ↓                                                            │
    async_read_some 回调触发时，访问 this->_socket               │
    → 空指针 / 野指针 → 崩溃 💥                                  │
我的想法是这里的self保证了不会_session.erase(session_id)直接引发错误，而是会等到异步读写都处理完毕，
    各个self使_session的引用计数归零才会最终析构。
我的理解二：正常情况下，每个回调的 self 保活到它自己结束，而结束前又启动了下一个回调的 self，所以整个异步链完成前 session 不会被析构
*/
void CSession::AsyncReadBody(int msg_len){
    auto self = shared_from_this();
    AsyncReadFull(msg_len,[this,self,msg_len](const boost::system::error_code& ec,std::size_t bytesTransfered){
        try{
            if(ec){
                LOGI << "handle read failed, error is " << ec.what() << std::endl;
                Close();
                _server->ClearSession(_session_id);
                return;
            }
            if(bytesTransfered < msg_len){
                LOGI << "read length not match, read [" << bytesTransfered << "] , total ["<< msg_len << "]" << std::endl;
                Close();
                _server->ClearSession(_session_id);
                return;
            }
            memcpy(_recv_msg_node->_data,_data,msg_len);
            _recv_msg_node->_cur_len += bytesTransfered;
            _recv_msg_node->_data[_recv_msg_node->_total_len] = '\0';
            LOGI << "receive data is " << _recv_msg_node->_data << std::endl;
            //此处将消息投递到逻辑队列中
            LogicSystem::GetInstance()->PostMsgToQue(std::make_shared<LogicNode>(shared_from_this(), _recv_msg_node));
            //继续监听头部接受事件
            AsyncReadHead();
        }catch(std::exception& e){
            LOGE << "Exception is :" << e.what() << std::endl;
        }
    });
}

//这里会保证一定读全，而且不超
void CSession::AsyncReadFull(std::size_t len,handler real_handler){
    memset(_data,0,MAX_LENGTH);
    AsyncReadLen(0,len,real_handler);
}

//读取指定字节数
void CSession::AsyncReadLen(std::size_t has_read_len, std::size_t total_len,handler real_handler){
    auto self = shared_from_this(); 
    //&has_read_len ×××××××××× 这里传引用绝对会有问题，因为是异步的，有可能作用域消失直接析构，你还用引用，那就绝对有问题
    _socket.async_read_some(boost::asio::buffer(_data + has_read_len,total_len - has_read_len)
    ,[self,has_read_len,total_len,real_handler,this](const boost::system::error_code& ec, std::size_t bytesTransfered){
        if(ec){
            // 出现错误，调用回调函数
            real_handler(ec, has_read_len + bytesTransfered);
            return;
        }
        // 这里实际上has_read_len + bytesTransfered == total_len也可以   防御性编程
        if(has_read_len + bytesTransfered >= total_len){
            real_handler(ec,bytesTransfered + has_read_len);
            // real_handler(ec,total_len);
            return;
        }
        //这里长度不够
        AsyncReadLen(has_read_len + bytesTransfered,total_len,real_handler);
    });
}
//一个_socket被close()两次，没事的。但是如果一次close()都由于多线程出现问题，这就有问题了。
/*为什么顺序 close 两次是安全的
    asio 的实现里，win_iocp_socket_service_base::close() 一进来就检查哨兵：

    // boost/asio/detail/win_iocp_socket_service_base.hpp（简化）
    void close(implementation_type& impl, ...){
        if (impl.socket_ != invalid_socket) {     // ← 哨兵：已关闭就直接跳过
            socket_ops::close(impl.socket_, ...);
            impl.socket_ = invalid_socket;        // ← 置为无效
        }
    }
    第一次 close 之后 impl.socket_ 变成 invalid_socket，第二次进来早退，根本不会下发系统调用。 */
void CSession::Close(){
    // std::lock_guard<std::mutex> guard_lock(_session_mtx);
    // _socket.close();
    /*// AsyncReadHead / AsyncReadBody 的所有错误分支都是这个形状
        Close();                              // ← 用的是 throwing 重载 _socket.close()
        _server->ClearSession(_session_id);   // ← 如果上一行抛了，这句就被跳过
虽然外层有 try{...}catch(std::exception&) 兜住（不会 terminate），但清理就漏了 → session 残留在 _sessions、uip_ 不删、计数不减。*/

    std::lock_guard<std::mutex> guard_lock(_session_mtx);
    boost::system::error_code ec;
    _socket.close(ec);                       // 不抛
    if(ec && ec != boost::asio::error::bad_descriptor){
        LOGI << "close socket error: " << ec.message()
             << " session_id=" << _session_id;
    }
}

// void CSession::DealExceptionSession(){

// }

//  version 1
// void CSession::Send(short msg_id,std::string msg_data){
//     std::unique_lock<std::mutex> unique_lock(_send_lock);
//     auto send_size = _send_que.size();
//     if(send_size > MAX_SENDQUE){
//         std::cerr << "ChatServer send_queue is full, can not add data" << std::endl;
//         return;
//     }
//     //假设一开始没数据，这里_send_que里才有第一个数据，当然要直接写呀  but 此时send_size是push之前的_send_que.size() 为 0
//     //所以这里 _send_que.size() = 1 ——> 写1
//     //        _send_que.size() > 1 ————>  写2（在写1中实现了）
//     _send_que.push(std::make_shared<SendNode>(msg_data.c_str(),msg_data.length(),msg_id));
//     if(send_size > 0){
//         return;
//     }
// 	auto& msgnode = _send_que.front();
//     boost::asio::async_write(_socket,boost::asio::buffer(msgnode->_data,msgnode->_total_len),
//     std::bind(HandleWrite,this,std::placeholders::_1,shared_from_this()));
// }

//version 2 这样写，感觉更好理解
void CSession::Send(short msg_id,std::string msg_data){
    if(msg_data.size() > MAX_LENGTH){
        LOGE << "Send 包体超长: " << msg_data.size()
             << " > " << MAX_LENGTH << ", msg_id=" << msg_id << "，已丢弃";
        return;                       // 宁可不发，也别把协议搞坏
    }
    std::unique_lock<std::mutex> unique_lock(_send_lock);
    if(_send_que.size() > MAX_SENDQUE){
        LOGE << "ChatServer send_queue is full, can not add data" << std::endl;
        return;
    }
    //所以这里 _send_que.size() = 1 ——> 写1
    //        _send_que.size() > 1 ————>  写2（在写1中实现了）
    _send_que.push(std::make_shared<SendNode>(msg_data.c_str(),msg_data.length(),msg_id));
    LOGI << "send msg_id=" << msg_id << " len=" << msg_data.size() << " uid=" << _user_uid;
    if(_send_que.size() > 1){
        return;
    }
	auto& msgnode = _send_que.front();
    //在 C++ 中，普通函数和成员函数的行为不一样：普通函数：函数名可以隐式转为指针
    //成员函数：函数名不能隐式转为指针   std::bind(HandleWrite,this,std::placeholders::_1,shared_from_this()) ××××××××
    boost::asio::async_write(_socket,boost::asio::buffer(msgnode->_data,msgnode->_total_len),
    std::bind(&HandleWrite,this,std::placeholders::_1,shared_from_this()));
}

//boost::system::error_code — asio 异步操作的错误   这是 async_write 操作本身的错误码，由 Boost.Asio 在回调中传入
//std::exception — C++ 异常     场景	                                说明
                    //memcpy 访问非法地址	                    _data 是空指针或者长度不对
                    // shared_from_this() 抛 bad_weak_ptr	    对象没有被 shared_ptr 管理就在用 enable_shared_from_this
void CSession::HandleWrite(const boost::system::error_code& error, std::shared_ptr<CSession> shared_self){
    try{
        if(!error){
            std::unique_lock<std::mutex> unique_lock(_send_lock);       //？？？？？这里锁好像用着有问题
            _send_que.pop();
            if(!_send_que.empty()){
                auto& msgnode = _send_que.front();
                boost::asio::async_write(_socket,boost::asio::buffer(msgnode->_data,msgnode->_total_len)
                ,std::bind(&HandleWrite,this,std::placeholders::_1,shared_from_this()));
            }
        }else{
            LOGE << "[ChatServer]error is :" << error.what() << std::endl;
        }
    }catch (std::exception& e){
        LOGE << "Exceptin code is :" << e.what() << std::endl;
    }
}

LogicNode::LogicNode(std::shared_ptr<CSession> session,std::shared_ptr<RecvNode> recvNode)
    :_csession(session),_recvNode(recvNode){

}

