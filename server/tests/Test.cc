#include "Test.h"

#include <fstream>  // std::ofstream：写临时配置文件用

#include "config.h"
#include "logger.h"

bool Test::run() const {
    // ================= 第 1 部分：测试日志模块 =================
    // 首次调用 getInstance 会创建单例，并把日志写到 /tmp（不污染仓库）
    Logger::getInstance("/tmp/smart_home_log_test.log");

    LOG_DEBUG("this is a debug message");
    LOG_INFO("this is an info message");
    LOG_WARN("this is a warn message");
    LOG_ERROR("this is an error message");

    Logger::destroy();  // 释放单例

    // ================= 第 2 部分：测试配置模块 =================
    // 先写一个临时配置文件到 /tmp，模拟 server.conf 的内容
    std::ofstream conf("/tmp/smart_home_test.conf");
    conf << "ip 192.168.1.10\n";           // ip 配置
    conf << "port 9000\n";                 // 端口（故意不用默认值，验证能读出来）
    conf << "thread_num 8\n";              // 线程数
    conf << "# 这是注释，应被忽略\n";        // 注释行，加载时应被跳过
    conf << "log_file /tmp/smart_home_test.log\n";  // 日志路径
    conf.close();  // 写完关闭，确保内容写入磁盘

    // 创建 Config 对象，并加载刚才写好的文件
    smart_home::Config cfg;
    if (!cfg.load("/tmp/smart_home_test.conf")) {
        return false;  // 加载失败，测试不通过
    }

    // 逐项校验：读出来的值必须和文件里写的一致，否则返回 false
    if (cfg.ip() != "192.168.1.10") return false;
    if (cfg.port() != 9000) return false;
    if (cfg.threadNum() != 8) return false;
    if (cfg.logFile() != "/tmp/smart_home_test.log") return false;
    // 文件里没写 task_num，应返回默认值 10000（验证"默认值兜底"能力）
    if (cfg.taskNum() != 10000) return false;

    return true;  // 全部通过
}
