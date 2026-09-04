cmake_minimum_required(VERSION 3.16)

project(server_tests LANGUAGES CXX)


# ============================================================
# 原有Server基础测试
# ============================================================

add_executable(
    server_test

    Test.cc

    test_main.cc

    # 测试日志模块
    ../src/logger.cc

    # 测试配置模块
    ../src/config.cc
)


target_link_libraries(
    server_test

    PRIVATE

    smart_home_common

    log4cpp

    pthread
)


target_compile_features(
    server_test
    PRIVATE
    cxx_std_11
)


target_include_directories(
    server_test

    PRIVATE

    ${CMAKE_CURRENT_SOURCE_DIR}

    ${CMAKE_CURRENT_SOURCE_DIR}/../include
)


add_test(
    NAME server_test
    COMMAND server_test
)



# ============================================================
# MySQL集成测试
# ============================================================
#
# 注意：
#
# mysql_test依赖：
#
# 1. 本机MySQL服务
# 2. server/conf/server.conf
# 3. 正确数据库账号密码
# 4. smarthome数据库
#
# 因此这里暂时不使用add_test()加入CTEST。
#
# 避免CI环境没有MySQL时导致整个CI失败。
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


add_executable(
    mysql_test

    mysql_test.cc

    # Config实现
    ../src/config.cc

    # MySQLClient实现
    ../src/MySQLClient.cc
)


target_link_libraries(
    mysql_test

    PRIVATE

    pthread

    ${MYSQLCLIENT_LIBRARY}
)


target_compile_features(
    mysql_test

    PRIVATE

    cxx_std_11
)


target_include_directories(
    mysql_test

    PRIVATE

    ${CMAKE_CURRENT_SOURCE_DIR}/../include
)

# ============================================================
# UserService 注册业务集成测试
#
# 测试内容：
#
# 1. 正常用户注册
# 2. 重复用户名
# 3. 非法参数
# 4. password_hash + salt 是否正确保存
#
# 这个测试依赖真实 MySQL，
# 因此暂时不加入 add_test()，
# 防止 CI 环境没有数据库配置导致失败。
# ============================================================

find_package(OpenSSL REQUIRED)


add_executable(
    user_service_test

    # UserService测试入口
    user_service_test.cc

    # 配置模块
    ../src/config.cc

    # MySQL访问层
    ../src/MySQLClient.cc

    # 用户业务层
    ../src/UserService.cc
)


target_link_libraries(
    user_service_test

    PRIVATE

    # 公共协议模块：
    # UserService中使用了ErrorCode
    smart_home_common

    # MySQL客户端库
    ${MYSQLCLIENT_LIBRARY}

    # PBKDF2 / RAND_bytes
    OpenSSL::Crypto

    # MySQLClient中使用mutex
    pthread
)


# 使用C++11
target_compile_features(
    user_service_test

    PRIVATE

    cxx_std_11
)


# 头文件搜索目录
target_include_directories(
    user_service_test

    PRIVATE

    # server/include/
    ${CMAKE_CURRENT_SOURCE_DIR}/../include

    # common/include/
    # 用于找到：
    # protocol/ErrorCode.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../../common/include
)


# ============================================================
# AuthHandler 注册消息集成测试
#
# 测试链路：
#
# TlvMessage -> AuthHandler -> UserService -> MySQL
#
# 该测试依赖真实 MySQL 与 server/conf/server.conf，因此不加入
# add_test()，避免无数据库的 CI 环境失败；需要时直接运行可执行文件。
# ============================================================

add_executable(
    auth_handler_test

    # AuthHandler 真实链路测试入口
    auth_handler_test.cc

    # 配置模块
    ../src/config.cc

    # MySQL 访问层
    ../src/MySQLClient.cc

    # 用户注册业务层
    ../src/UserService.cc

    # 认证消息业务处理层
    ../src/AuthHandler.cc
)


target_link_libraries(
    auth_handler_test

    PRIVATE

    # AuthProtocol / TlvMessage / ErrorCode 等公共协议实现
    smart_home_common

    # MySQL C API
    ${MYSQLCLIENT_LIBRARY}

    # UserService 的 PBKDF2 与安全随机数
    OpenSSL::Crypto

    # MySQLClient 的 std::mutex
    pthread
)


target_compile_features(
    auth_handler_test

    PRIVATE

    cxx_std_11
)


target_include_directories(
    auth_handler_test

    PRIVATE

    # server/include/
    ${CMAKE_CURRENT_SOURCE_DIR}/../include

    # common/include/；同时由 smart_home_common 以 PUBLIC 方式导出。
    ${CMAKE_CURRENT_SOURCE_DIR}/../../common/include
)