cmake_minimum_required(VERSION 3.16)

project(server LANGUAGES CXX)

add_executable(smart_home_server
    # --------------------------------------------------------
    # A：服务器基础设施（仅保留既有源文件，不修改其实现）
    # --------------------------------------------------------
    src/main.cc
    src/logger.cc
    src/config.cc
    src/thread_pool.cc
    src/reactor.cc
    src/connection.cc

    # --------------------------------------------------------
    # B：MySQL 数据访问层
    # --------------------------------------------------------
    src/MySQLClient.cc
    src/UserService.cc
    src/AuthHandler.cc
    src/DeviceService.cc
    src/RecordService.cc
    src/ResourceHandler.cc
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
