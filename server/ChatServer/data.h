#pragma once
#include <string>
#include <vector>
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