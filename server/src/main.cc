// ============================================================================
// main.cc —— Server 启动入口（角色 A 负责）
//
// 启动流程：
//   1. 确定配置文件路径（默认 server/conf/server.conf，可用命令行参数覆盖）
//   2. 加载配置（加载失败就用默认值，不阻止启动）
//   3. 确保日志目录存在 -> 初始化日志（之后所有输出都走 LOG_xxx 宏）
//   4. 打印启动信息（把读到的配置回显出来，方便确认）
//   5. （未来第 2 天：这里启动 Reactor + ThreadPool，开始监听端口）
//   6. 优雅退出：释放日志单例
// ============================================================================

#include <sys/stat.h>   // mkdir：创建目录（POSIX 系统调用）
#include <sys/types.h>  // mkdir 相关类型定义

#include <iostream>     // std::cerr：标准错误输出
#include <string>       // std::string、std::to_string（int 转字符串）

#include "config.h"     // smart_home::Config 配置模块
#include "logger.h"     // Logger 单例 和 LOG_xxx 宏

// ---------------------------------------------------------------------------
// 确保日志文件的父目录存在。
// 例：log_file = "./log/server.log" -> 创建 "./log" 目录。
// 为什么需要：如果目录不存在，log4cpp 打不开日志文件，文件输出会失效。
// ---------------------------------------------------------------------------
static void ensureLogDir(const std::string &log_file) {
    // find_last_of 找最后一个 '/' 的位置；npos 表示没找到
    size_t pos = log_file.find_last_of('/');
    if (pos == std::string::npos) {
        return;  // 没有 '/'，说明日志文件就在当前目录，无需建目录
    }
    // 截取最后一个 '/' 之前的部分，就是目录路径
    std::string dir = log_file.substr(0, pos);
    // mkdir(路径, 权限)：0755 = 所有者可读写执行，其他人可读可执行
    // 目录已存在时 mkdir 会失败（errno == EEXIST），这里直接忽略即可
    mkdir(dir.c_str(), 0755);
}

int main(int argc, char *argv[]) {
    // ---- 第 1 步：确定配置文件路径 ----
    // 默认路径：server/conf/server.conf（约定：从项目根目录启动服务器）
    std::string config_path = "server/conf/server.conf";
    if (argc > 1) {
        // 如果启动时带了参数，就用参数指定的配置文件
        // 例如：./smart_home_server /tmp/test.conf
        config_path = argv[1];
    }

    // ---- 第 2 步：加载配置 ----
    smart_home::Config cfg;
    if (!cfg.load(config_path)) {
        // 此时日志还没初始化，只能用标准错误流打印警告
        // 注意：不退出！Config 的所有取值方法都带默认值，服务器照样能启动
        std::cerr << "[warn] cannot load config: " << config_path
                  << ", using default values" << std::endl;
    }

    // ---- 第 3 步：确保日志目录存在，再初始化日志 ----
    ensureLogDir(cfg.logFile());
    // 首次调用 getInstance：创建 Logger 单例，日志写到配置指定的文件
    // 从这一行开始，LOG_xxx 宏同时输出到【控制台 + 日志文件】
    Logger::getInstance(cfg.logFile());

    // ---- 第 4 步：打印启动信息（把读到的配置回显出来）----
    // 注意：LOG_xxx 宏的参数是 const char*，所以要用
    // ("字符串字面量" + std::string).c_str() 把整行拼成一个 C 字符串
    LOG_INFO("========== SmartHome Server starting ==========");
    LOG_INFO(("config file : " + config_path).c_str());
    LOG_INFO(("listen ip   : " + cfg.ip()).c_str());
    // std::to_string(int)：把整数转成字符串，才能用 + 拼接
    LOG_INFO(("listen port : " + std::to_string(cfg.port())).c_str());
    LOG_INFO(("thread num  : " + std::to_string(cfg.threadNum())).c_str());
    LOG_INFO(("task num    : " + std::to_string(cfg.taskNum())).c_str());
    LOG_INFO(("video path  : " + cfg.videoPath()).c_str());
    LOG_INFO(("log file    : " + cfg.logFile()).c_str());

    // ---- 第 5 步：主循环占位 ----
    // 第 2 天会在这里创建 Reactor + ThreadPool，开始 accept 客户端连接
    LOG_INFO("server is running (skeleton)");

    // ---- 第 6 步：优雅退出 ----
    // 真实服务器会在这里等待退出信号；现在直接走清理流程
    LOG_INFO("server exit");
    Logger::destroy();  // 释放日志单例，刷新并关闭日志文件
    return 0;
}