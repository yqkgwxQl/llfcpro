#include "ConfigMgr.h"
#include "Logger.h"

/*  [Database]
    Host=127.0.0.1
    Port=3306

    [App]
    Name=MyGame

    根节点 (Root)
        子节点 "Database" (对应 [Database])
            子节点 "Host" -> 值 "127.0.0.1"
            子节点 "Port" -> 值 "3306"
        子节点 "App" (对应 [App])
            子节点 "Name" -> 值 "MyGame"*/
std::string ConfigMgr::_override_path;  //静态成员的定义
ConfigMgr::ConfigMgr(){
    try{
        //获取当前的工作目录
        boost::filesystem::path current_path = boost::filesystem::current_path();
        //构建config.ini的完整路径 使用了运算符重载
        //-------------这里需要注意可执行程序在build目录下，而config.ini文件在build目录的上级目录
        // boost::filesystem::path config_path = current_path.parent_path() / "config.ini";
        boost::filesystem::path config_path = _override_path.empty() 
            ? boost::filesystem::current_path() / "config.ini"  
            : boost::filesystem::path(_override_path);  // boost::filesystem::path() ————> 返回类型为path
        LOGI << "Config path: " << config_path << std::endl;

        //使用boost.PropertyTree来读取ini文件 ————> 将磁盘上的文本文件（config.ini）解析并加载到内存中的一个树状数据结构中
        boost::property_tree::ptree pt; 
        boost::property_tree::read_ini(config_path.string(),pt);
        
        //遍历ini文件中所有的section
        for(const auto& section_pair : pt){
            const std::string& section_name = section_pair.first;
            const boost::property_tree::ptree& section_tree = section_pair.second;

            //对于每个section，遍历其所有的key-value对
            SectionInfo sectionInfo;
            for(const auto& key_value_pair : section_tree){
                const std::string& key = key_value_pair.first;
                const std::string& value = key_value_pair.second.get_value<std::string>();
                sectionInfo._section_datas[key] = value;
            }
            _config_map[section_name] = sectionInfo;
        }

        // 输出所有的section和key-value对  
        // for(const auto& section : _config_map){
        //     std::string section_name = section.first;
        //     SectionInfo section_config = section.second;
        //     std::cout << "[" << section_name << "]" << std::endl;
        //     for(const auto& key_value_pair : section_config._section_datas){
        //         std::cout << key_value_pair.first << "=" << key_value_pair.second << std::endl;
        //     }
        // }
    }catch (const std::exception& e) {
        LOGE << "Error loading config: " << e.what() << std::endl;
    }
}

ConfigMgr::~ConfigMgr(){
    _config_map.clear();
}

SectionInfo ConfigMgr::operator[](const std::string& key){
    // if(_config_map.find(key) == _config_map.end()){
    //     return SectionInfo();
    // }
    // return _config_map[key];
    auto it = _config_map.find(key);
    if (it != _config_map.end()) {
        return it->second;
    }
    return SectionInfo();  // 按值返回空字符串，安全
}

ConfigMgr& ConfigMgr::GetInstance(){
//补充
//1.这个静态变量，生命周期随整个进程；但只在此函数这个作用域可见
//2.这里C++11之后线程安全(如果多个线程同时执行到静态局部变量的声明处，只有一个线程会执行初始化，其他线程会阻塞等待，直到初始化完成。)
    static ConfigMgr cfg_mgr;
    return cfg_mgr;
}

void ConfigMgr::SetConfigPath(const std::string& path){
    _override_path = path;
}

SectionInfo::~SectionInfo(){
    _section_datas.clear();
}
SectionInfo::SectionInfo(const SectionInfo& src){
    _section_datas = src._section_datas;
}
SectionInfo& SectionInfo::operator=(const SectionInfo& src){
    if(&src == this){
        // return src;//这里不能扩大权限
        return *this;
    }
    _section_datas = src._section_datas;
    return *this;
}

std::string SectionInfo::operator[](const std::string& key){
    //find() 查找一次，_section_datas[key] 又查找一次，效率低。
    // if(_section_datas.find(key) == _section_datas.end()){
    //     return "";
    // }
    // return _section_datas[key];
    auto it = _section_datas.find(key);
    if (it != _section_datas.end()) {
        return it->second;
    }
    return "";  // 按值返回空字符串，安全
}

    
