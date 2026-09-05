#include "config.h"

#include <cstddef>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>



namespace smart_home{
    // 加载配置文件
    bool Config::load(const std::string &filepath){
        // 使用 ifstream 打开文件 只读文本模式
        std::ifstream in(filepath.c_str());
        if(!in.is_open()){ // 打开失败
            return false;
        }
        std::string line;
        // 一次读一行
        while(std::getline(in, line)){ 
            // 1.去掉 注释
            size_t pos = line.find('#');
            if(pos != std::string::npos){
                // 保留 #之前的部分 后面内容都丢掉
                line = line.substr(0, pos);
            }

            // 2.拆分key
            std::istringstream iss(line);
            std::string key;
            if(!(iss >> key)){
                continue;
            }

            // 3.拆出value
            std::string value;
            std::getline(iss, value);

            // 去掉value首尾空白字符
            size_t first = value.find_first_not_of(" \t"); 
            size_t last = value.find_last_not_of(" \t"); 
            if(first == std::string::npos){
                value.clear();
            }else{
                value = value.substr(first, last-first + 1);
            }

            // 4.存入map
            // key存在 覆盖旧值
            _items[key] = value;
        }
        return true;
    }

    std::string Config::getString(const std::string &key, const std::string &def) const {
        // 在 items_ 这个 map 里查找 key
        std::map<std::string, std::string>::const_iterator it = _items.find(key);
        if (it != _items.end()) {
            // 找到了：it->first 是 key，it->second 是 value
            return it->second;
        }
        return def;  // 没找到，返回调用者给的默认值
    }

    // 根据key获取int类型值
    int Config::getInt(const std::string& key, int def) const{
        std::map<std::string, std::string>::const_iterator it = _items.find(key);
        if(it == _items.end()){
            return def; // 未找到返回默认值
        }
        // atoi 是 "8000" 转换为8000
        return std::atoi(it->second.c_str());
    }

    std::string Config::ip() const{
        return getString("ip","127.0.0.1");
    }

    int Config::port() const{
        return getInt("port", 7777);
    }
    int Config::threadNum() const{
        return getInt("thread_num", 4);
    }
    int Config::taskNum() const{
        return getInt("task_num", 10000);
    }

    std::string Config::videoPath() const{
        return getString("video_path","./data/");
    }
    std::string Config::logFile() const{
        return getString("log_file","./log/server.log");
    } 

    //MySQL 配置
    std::string Config::mysqlHost() const{
        return getString("mysql_host", "127.0.0.1");
    }

    int Config::mysqlPort() const{
        return getInt("mysql_port", 3306);
    }

    std::string Config::mysqlUser() const{
        return getString("mysql_user", "");
    }

    std::string Config::mysqlPassword() const{
        return getString("mysql_password","");
    }

    std::string Config::mysqlDatabase() const{
        return getString("mysql_database","smarthome");
    }
    
    
} // namespace smart_home
