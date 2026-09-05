cmake_minimum_required(VERSION 3.16)

project(server LANGUAGES CXX)

add_executable(smart_home_server
    src/main.cc # 主程序入口
    src/logger.cc # 日志模块
    src/config.cc # 配置模块
    src/thread_pool.cc # 线程池模块
    src/reactor.cc # 网络模块
    src/connection.cc # 连接模块
    src/stream_session.cc #流会话模块

    src/media/mock_media_source.cc # 假媒体源（测试用，不依赖 FFmpeg）

    # --------------------------------------------------------
    # A：服务器启动入口
    # --------------------------------------------------------
    src/main.cc


    # --------------------------------------------------------
    # A：日志模块
    # --------------------------------------------------------
    src/logger.cc


    # --------------------------------------------------------
    # A：配置模块
    # --------------------------------------------------------
    src/config.cc


    # --------------------------------------------------------
    # B：MySQL 数据访问层
    # --------------------------------------------------------
    src/MySQLClient.cc


    # --------------------------------------------------------
    # B：用户注册 / 登录业务层
    # --------------------------------------------------------
    src/UserService.cc


    # --------------------------------------------------------
    # C：认证消息业务处理层
    #
    # 负责将 REGISTER_REQUEST 的 TLV 消息连接到 UserService。
    # --------------------------------------------------------
    src/AuthHandler.cc
)

# ============================================================
# 查找 OpenSSL
#
# UserService 中密码安全处理使用：
#
# RAND_bytes()
# PKCS5_PBKDF2_HMAC()
# EVP_sha256()
#
# 这些实现位于 libcrypto 中。
# ============================================================

find_package(
    OpenSSL REQUIRED
)


# ============================================================
# 查找 MySQL Client 库
#
# MySQLClient 中使用：
#
# mysql_init()
# mysql_real_connect()
# mysql_real_query()
# mysql_close()
# ============================================================

find_library(
    MYSQLCLIENT_LIBRARY
    mysqlclient
)


if(NOT MYSQLCLIENT_LIBRARY)

    message(
        FATAL_ERROR
        "libmysqlclient not found"
    )

endif()

target_link_libraries(
    smart_home_server

    PRIVATE

    # 公共 TLV 协议模块
    smart_home_common


    # 日志
    log4cpp


    # std::thread / pthread
    pthread


    # MySQL C API
    ${MYSQLCLIENT_LIBRARY}


    # UserService 密码哈希、随机 salt
    OpenSSL::Crypto
)


# ============================================================
# 使用 C++11
# ============================================================

target_compile_features(
    smart_home_server

    PRIVATE

    cxx_std_11
)


# ============================================================
# Server 头文件目录
#
# 这样源码中可以直接：
#
# #include "config.h"
# #include "MySQLClient.h"
# #include "UserService.h"
# ============================================================

target_include_directories(
    smart_home_server

    PRIVATE

    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# 可执行文件输出到项目根目录
set_target_properties(smart_home_server PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}
)
