#pragma once

#include <string>
#include <fstream>  
#include <boost/property_tree/ptree.hpp>  
#include <boost/property_tree/ini_parser.hpp>  
#include <boost/filesystem.hpp>    
#include <map>
#include <iostream>

/*SectionInfo <===>
    [GateServer]下的
    Port = 8080
    Port2 = 8900
    sectionInfo["Port"] = 8080
    sectionInfo["Port2"] = 8900
ConfigMgr <===>
    [GateServer]
    Port = 8080
    [VarifyServer]
    Port = 50051
    ConfigMgr["GateServer"] = sectionInfo1
    ConfigMgr["VarifyServer"] = sectionInfo2*/
struct SectionInfo{

    SectionInfo() = default;
    ~SectionInfo();
    SectionInfo(const SectionInfo& src);
    SectionInfo& operator=(const SectionInfo& src);
    std::string operator[](const std::string& key);

    std::map<std::string,std::string> _section_datas;
};

class ConfigMgr{

public:
    ~ConfigMgr();
    ConfigMgr(const ConfigMgr& src) = delete;
    ConfigMgr& operator=(const ConfigMgr& src) = delete;
    SectionInfo operator[](const std::string& key);
    static ConfigMgr& GetInstance();
private:
    ConfigMgr();
    std::map<std::string,SectionInfo> _config_map;
};