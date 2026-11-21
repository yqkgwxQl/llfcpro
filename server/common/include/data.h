#pragma once
#include <string>
#include <vector>
#include "const.h"

struct UserInfo {
	UserInfo():name(""), pwd(""),uid(0),email(""),nick(""),desc(""),sex(0), icon(""), back("") {}
	std::string name;
	std::string pwd;
	int uid;
	std::string email;
	std::string nick;
	std::string desc;
	int sex;
	std::string icon;
	std::string back;
};

//？？？？？？？？？？？？代码的复用性不足，以后进行解决
struct ApplyInfo{
	ApplyInfo(int uid, std::string name, std::string desc, std::string icon, std::string nick, int sex, int status, int id):
	uid_(uid), name_(name), desc_(desc), icon_(icon), nick_(nick), sex_(sex), status_(status), id_(id){

	}
	
	int uid_;
	std::string name_;
	std::string desc_;
	std::string icon_;
	std::string nick_;
	int sex_;
	int status_;

	int id_; 	//用于动态获取applylist，见客户端applyfriendpage.cc代码和服务端LogicSystem.cc的GetApplyList代码
};

struct FriendInfo{
	FriendInfo(int uid, std::string name, std::string desc, std::string icon, std::string nick, int sex, int id):
	uid_(uid), name_(name), desc_(desc), icon_(icon), nick_(nick), sex_(sex), id_(id){

	}
	
	int uid_;
	std::string name_;
	std::string desc_;
	std::string icon_;
	std::string nick_;
	int sex_;

	int id_; 	//用于动态获取friendlist，同上面的ApplyInfo
};

struct ChatMsgRecord{
	ChatMsgRecord(long long id, std::string unique_id, int from_uid, int to_uid, long long thread_id, ChatMsgType msg_type, 
		std::string content, std::string chat_time, int delivered)
			:_id(id), _unique_id(unique_id), _from_uid(from_uid), _to_uid(to_uid), _thread_id(thread_id), _msg_type(msg_type),
			_content(content), _chat_time(chat_time), _delivered(delivered){

	}
	
	long long _id;			// id_ 由数据库 AUTO_INCREMENT 分配：
								//   · 写入路径（SaveChatMessages）：保持 0，INSERT 语句里不出现 id 列
								//   · 读取路径（GetOfflineMessages）：从结果集填充，同时作为离线拉取的分页游标
	std::string _unique_id;
	int _from_uid;
	int _to_uid;
	long long _thread_id;
	ChatMsgType _msg_type;
	std::string _content;
	std::string _chat_time;
	int _delivered;		// 0=待投递 1=已投递给接收方
};