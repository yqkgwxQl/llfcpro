#include "userdata.h"

SearchInfo::SearchInfo(int uid, QString name,QString nick, QString desc, int sex, QString icon)
    :_uid(uid),_name(name), _nick(nick),_desc(desc),_sex(sex),_icon(icon){
}

UserData::UserData() {}
