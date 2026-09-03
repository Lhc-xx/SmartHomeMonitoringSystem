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

        std::string ip() const;
        int port() const;
        int threadNum() const;
        int taskNum() const;
        std::string videoPath() const;
        std::string logFile() const;

    private:
        // 存数据
        std::map<std::string, std::string> _items;
    };
} // namespace smart_home

#endif //  CONFIG_H