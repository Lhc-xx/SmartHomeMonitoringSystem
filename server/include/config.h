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

        // 登录会话超时（秒），默认 1800（30 分钟）
        int sessionTimeout() const;

        //MySQL 配置
        //MySQL 服务器地址 默认127.0.0.1
        std::string mysqlHost() const;

        //MySQL 端口
        //默认 3306
        int mysqlPort() const;

        //MySQL 用户名
        std::string mysqlUser() const;

        //MySQL 密码
        std::string mysqlPassword() const;

        //项目使用的 数据库名称
        //默认 smarthome
        std::string mysqlDatabase() const;

        //摄像头 HTTP 接口 token 签名密钥（迅思维默认 secret，可被 server.conf 覆盖）
        std::string cameraSecret() const;
    private:
        // 存数据
        //key ----> value
        std::map<std::string, std::string> _items;
    };
} // namespace smart_home

#endif //  CONFIG_H