#include "Test.h"

#include "../include/logger.h"

bool Test::run() const {
    // 首次 getInstance 创建单例并绑定日志文件（写 /tmp，不污染仓库）。
    Logger::getInstance("/tmp/smart_home_log_test.log");

    LOG_DEBUG("this is a debug message");
    LOG_INFO("this is an info message");
    LOG_WARN("this is a warn message");
    LOG_ERROR("this is an error message");

    Logger::destroy();
    return true;
}