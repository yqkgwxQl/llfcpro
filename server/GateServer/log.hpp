#pragma once

//?????????????????????????????????????????
#include<iostream>
#include<string>
#include<cstdarg>
#include<ctime>
#include<unistd.h>

#define DEBUG    0
#define NORMAL   1
#define WARNING  2
#define ERROR    3
#define FATAL    4

#define LOG_ERR "log.err"
#define LOG_NORMAL "log.txt"

const char* to_level_str(int level){
    switch(level){
        case 0 :
        return "DEBUG";
        case 1 :
        return "NORMAL";
        case 2 :
        return "WARNING";
        case 3 :
        return "ERROR";
        case 4 :
        return "FATAL";
        default:return nullptr;
    }
}
void logMessage(int level , const char* format , ...){
    //[日志等级][时间][pid][message]

    // va_list
    char logprefix[1024];
    time_t now = time(nullptr);
    struct tm * ptm = localtime(&now);
    snprintf(logprefix,sizeof(logprefix),"[%s][%d-%d-%d-%d-%d-%d][pid:%d]",to_level_str(level),ptm->tm_year+1900,ptm->tm_mon+1,ptm->tm_mday,ptm->tm_hour,ptm->tm_min,ptm->tm_sec,getpid());
    char logcontent[1024];
    va_list arg;
    va_start(arg,format);

    vsnprintf(logcontent,sizeof(logcontent),format,arg);
    va_end(arg);
    // std::cout << logprefix << "[" << logcontent << "]" << std::endl;
    FILE* log = fopen(LOG_NORMAL,"a");
    FILE* err = fopen(LOG_ERR,"a");
    if( log != nullptr && err != nullptr){
        FILE* cur = nullptr;
        if(level == NORMAL || level == WARNING || level == DEBUG)
            cur = log;
        if(level == ERROR || level == FATAL)
            cur = err;
        if(cur)
            fprintf(cur,"%s[%s]\n",logprefix,logcontent);

        fclose(log);
        fclose(err);
    } 
}

