#ifndef LOGGER_H
#define LOGGER_H

#include <iostream>
#include <string>

#include <log4cpp/OstreamAppender.hh>
#include <log4cpp/FileAppender.hh>
#include <log4cpp/PatternLayout.hh>
#include <log4cpp/Category.hh>

using namespace std;
using namespace log4cpp;

// 像 printf 一样使用，日志自动带文件名、函数名、行号。
#define LOG_WARN(msg)  Logger::getInstance()->warn(__FILE__, __LINE__, __FUNCTION__, msg)
#define LOG_ERROR(msg) Logger::getInstance()->error(__FILE__, __LINE__, __FUNCTION__, msg)
#define LOG_DEBUG(msg) Logger::getInstance()->debug(__FILE__, __LINE__, __FUNCTION__, msg)
#define LOG_INFO(msg)  Logger::getInstance()->info(__FILE__, __LINE__, __FUNCTION__, msg)

// log4cpp 单例封装
// - 记录器：Category root，子模块继承其配置
// - 过滤器：Priority，低于阈值的日志被过滤
// - 格式化器：PatternLayout "%d %c [%p] %m%n"
// - 输出器：OstreamAppender(控制台) + FileAppender(文件) 双输出
class Logger{
public:
    void warn(const char *file, int line, const char * func, const char* msg);
    void error(const char *file, int line, const char * func, const char* msg);
    void debug(const char *file, int line, const char * func, const char* msg);
    void info(const char *file, int line, const char * func, const char* msg);

    // 首次调用创建单例并绑定日志文件  之后调用直接返回已有实例
    static Logger *getInstance(const std::string &log_file = "server.log");

    // 释放单例 进程退出时调用
    static void destroy();

private:
    explicit Logger(const std::string &log_file);
    // 析构私有
    ~Logger();
    // 删除复制语义
    Logger(const Logger&) = delete;
    Logger operator=(const Logger&) = delete;

private:
    static Logger *_pInstance; // 单例对象
    static Category *_root;
};

#endif // LOGGER_H