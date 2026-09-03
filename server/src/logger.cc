#include "../include/logger.h"

#include <cstddef>
#include <cstdio>
#include <map>
#include <string>

Logger *Logger::_pInstance = nullptr;
Category *Logger::_root = nullptr;

Logger::Logger(const std::string &log_file){
    // 输出器 1.控制台
    OstreamAppender * consoleAppender = new OstreamAppender("console", &std::cout);
    PatternLayout *consoleLayout = new PatternLayout();
    consoleLayout->setConversionPattern("%d %c [%p] %m%n");
    consoleAppender->setLayout(consoleLayout);

    // 输出器 2：日志文件
    FileAppender* fileAppender = new FileAppender("file", log_file.c_str());
    PatternLayout *fileLayout = new PatternLayout();
    fileLayout->setConversionPattern("%d %c [%p] %m%n");
    fileAppender->setLayout(fileLayout);

    // 记录器 root分类， 所以子模块继承ta的优先级和输出目的地
    _root = &Category::getRoot();
    _root->setPriority(Priority::DEBUG);
    _root->addAppender(consoleAppender);
    _root->addAppender(fileAppender);
}

Logger::~Logger(){
    if (_root != nullptr) {
        // 这个版本的 Category 没有返回 map 的 getAppenders()，
        // 按名字取出两个 Appender，从 root 上移除后再删除。
        Appender *consoleAppender = _root->getAppender("console");
        Appender *fileAppender = _root->getAppender("file");
        _root->removeAllAppenders();
        // delete consoleAppender;
        // delete fileAppender;
        _root = nullptr;
    }
}

// 获取单例对象
Logger *Logger::getInstance(const std::string &log_file){
    if(_pInstance == nullptr){
        _pInstance = new Logger(log_file);
    }
    return _pInstance;
}

void Logger::destroy(){
    if(_pInstance != nullptr){
        delete _pInstance;
        _pInstance = nullptr;
    }
}

void Logger::debug(const char *file, int line, const char *func, const char *msg){
    char buffer[1024];
    std::snprintf(buffer, sizeof(buffer), "%s:%d %s() %s",
                    file, line, func, msg);
    _root->debug(buffer);
}

void Logger::info(const char *file, int line, const char *func, const char *msg){
    char buffer[1024];
    std::snprintf(buffer, sizeof(buffer), "%s:%d %s() %s",
                    file, line, func, msg);
    _root->info(buffer);
}


void Logger::warn(const char *file, int line, const char *func, const char *msg){
    char buffer[1024];
    std::snprintf(buffer, sizeof(buffer), "%s:%d %s() %s",
                    file, line, func, msg);
    _root->warn(buffer);
}

void Logger::error(const char *file, int line, const char *func, const char *msg){
    char buffer[1024];
    std::snprintf(buffer, sizeof(buffer), "%s:%d %s() %s",
                    file, line, func, msg);
    _root->error(buffer);
}


