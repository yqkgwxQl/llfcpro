#include "MsgNode.h"
#include <boost/asio.hpp>
#include "const.h"

MsgNode::MsgNode(short max_len):_total_len(max_len),_cur_len(0){
    _data = new char[_total_len + 1];
    _data[_total_len] = '\0';
}

void MsgNode::Clear(){
    memset(_data,0,_total_len);
    _cur_len = 0;
}

RecvNode::RecvNode(short max_len,short msg_id):MsgNode(max_len),_msg_id(msg_id){

}

short RecvNode::GetMsgId(){
    return _msg_id;
}

//      max_len + HEAD_TOTAL_LEN  ————>  这里非常关键，发送信息，一定要发送 id+len+data
SendNode::SendNode(const char* msg,short max_len,short msg_id):MsgNode(max_len + HEAD_TOTAL_LEN),_msg_id(msg_id){
    short id_host_to_net = boost::asio::detail::socket_ops::host_to_network_short(_msg_id);
    short len_host_to_net = boost::asio::detail::socket_ops::host_to_network_short(max_len);
    memcpy(_data,&id_host_to_net,HEAD_ID_LEN);
    memcpy(_data+HEAD_ID_LEN,&len_host_to_net,HEAD_DATA_LEN);
    //为什么这里_data不需要进行序列化？ ——————>  答案就一句话：单字节数据不存在字节序问题。
    //所以 SendNode 不负责序列化，它只负责包装（加个二进制头）。真正的序列化在调用方已经完成了
    //这里使用SendNode时，传入的参数msg已经是序列化的数据了，因为结构化数据不可能以字符串的形式呈现。
    memcpy(_data+HEAD_TOTAL_LEN,msg,max_len);
}

short SendNode::GetMsgId(){
    return _msg_id;
}


