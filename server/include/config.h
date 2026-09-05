#ifndef CONFIG_H
#define CONFIG_H

#include <map>
#include <string>

namespace smart_home {
    class Config{
    public:
        // 加载配置文件
        bool load(const std::string &filepath);

        // 根据key获取值  不存在时默认"" 或0
        std::string getString(const std::string &key, const std::string &def = "") const;
        int getInt(const std::string &key, int def = 0) const;

        //Server 基础配置
        std::string ip() const;
        int port() const;
        int threadNum() const;
        int taskNum() const;
        std::string videoPath() const;
        std::string logFile() const;

        //MySQL 配置
        //MySQL 服务器地址 默认127.0.0.1
        std::string mysqlHost() const;

        //MySQL 端口
        //默认 3306
        int mysqlPort() const;

        //MySQL 用户名
        std::string mysqlUser() const;

        //MySQL 密码
        //注意真实的密码只能放在本地的server.conf
        //不允许提交Git
        std::string mysqlPassword() const;

        //项目使用的 数据库名称
        //默认 smarthome
        std::string mysqlDatabase() const;
    private:
        // 存数据
        //key ----> value
        std::map<std::string, std::string> _items;
    };
} // namespace smart_home

#endif //  CONFIG_H