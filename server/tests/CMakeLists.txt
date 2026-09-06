cmake_minimum_required(VERSION 3.16)
project(server_tests LANGUAGES CXX)

#[[
真实数据库测试会创建并清理临时业务数据，且依赖未提交的 server.conf。
默认只生成可执行文件；交付或联调环境显式开启后才注册进 CTest。
]]
option(SMARTHOME_ENABLE_DB_INTEGRATION_TESTS
       "Enable B member integration tests that use a real MySQL database" OFF)

#[[基础服务器测试保留原有 CTest 入口，但不启动服务器进程。]]
add_executable(server_test
    Test.cc
    test_main.cc
    ../src/logger.cc
    ../src/config.cc
)
target_link_libraries(server_test PRIVATE smart_home_common log4cpp pthread)
target_compile_features(server_test PRIVATE cxx_std_11)
target_include_directories(server_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
)
add_test(NAME server_test COMMAND server_test)

#[[数据库测试需要本地 MySQL 和 server.conf，只生成可执行文件，不自动加入 CTest。]]
find_library(MYSQLCLIENT_LIBRARY mysqlclient)
if(NOT MYSQLCLIENT_LIBRARY)
    message(FATAL_ERROR "libmysqlclient not found")
endif()
find_package(OpenSSL REQUIRED)

set(SERVER_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../include)
set(COMMON_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../common/include)

add_executable(mysql_test mysql_test.cc ../src/config.cc ../src/MySQLClient.cc)
target_link_libraries(mysql_test PRIVATE ${MYSQLCLIENT_LIBRARY} pthread)
target_compile_features(mysql_test PRIVATE cxx_std_11)
target_include_directories(mysql_test PRIVATE ${SERVER_INCLUDE_DIR} ${COMMON_INCLUDE_DIR})

add_executable(user_service_test
    user_service_test.cc ../src/config.cc ../src/MySQLClient.cc ../src/UserService.cc)
target_link_libraries(user_service_test PRIVATE smart_home_common ${MYSQLCLIENT_LIBRARY}
    OpenSSL::Crypto pthread)
target_compile_features(user_service_test PRIVATE cxx_std_11)
target_include_directories(user_service_test PRIVATE ${SERVER_INCLUDE_DIR} ${COMMON_INCLUDE_DIR})

add_executable(auth_handler_test
    auth_handler_test.cc ../src/config.cc ../src/MySQLClient.cc ../src/UserService.cc
    ../src/AuthHandler.cc)
target_link_libraries(auth_handler_test PRIVATE smart_home_common ${MYSQLCLIENT_LIBRARY}
    OpenSSL::Crypto pthread)
target_compile_features(auth_handler_test PRIVATE cxx_std_11)
target_include_directories(auth_handler_test PRIVATE ${SERVER_INCLUDE_DIR} ${COMMON_INCLUDE_DIR})

add_executable(resource_handler_test
    resource_handler_test.cc ../src/config.cc ../src/MySQLClient.cc ../src/UserService.cc
    ../src/DeviceService.cc ../src/RecordService.cc ../src/ResourceHandler.cc)
target_link_libraries(resource_handler_test PRIVATE smart_home_common ${MYSQLCLIENT_LIBRARY}
    OpenSSL::Crypto pthread)
target_compile_features(resource_handler_test PRIVATE cxx_std_11)
target_include_directories(resource_handler_test PRIVATE ${SERVER_INCLUDE_DIR} ${COMMON_INCLUDE_DIR})

if(SMARTHOME_ENABLE_DB_INTEGRATION_TESTS)
    add_test(NAME mysql_test COMMAND mysql_test)
    add_test(NAME user_service_test COMMAND user_service_test)
    add_test(NAME auth_handler_test COMMAND auth_handler_test)
    add_test(NAME resource_handler_test COMMAND resource_handler_test)

    #[[
    四个测试按项目根目录解析 server/conf/server.conf；显式设置工作目录
    可保证从 build 目录运行 ctest 时与手工从源码根目录运行结果一致。
    ]]
    get_filename_component(SMARTHOME_PROJECT_ROOT
        "${CMAKE_CURRENT_SOURCE_DIR}/../.." ABSOLUTE)
    set_tests_properties(
        mysql_test
        user_service_test
        auth_handler_test
        resource_handler_test
        PROPERTIES
            WORKING_DIRECTORY "${SMARTHOME_PROJECT_ROOT}"
            LABELS "b;database;integration"
    )
endif()

#[[连接级登录状态机（角色 A）：不依赖 MySQL / log4cpp / epoll，自动加入 CTest。]]
add_executable(connection_session_test
    connection_session_test.cc ../src/connection.cc)
target_link_libraries(connection_session_test PRIVATE smart_home_common pthread)
target_compile_features(connection_session_test PRIVATE cxx_std_11)
target_include_directories(connection_session_test PRIVATE ${SERVER_INCLUDE_DIR} ${COMMON_INCLUDE_DIR})
add_test(NAME connection_session_test COMMAND connection_session_test)

#[[未登录拦截策略纯逻辑（角色 A）：不依赖第三方库，自动加入 CTest。]]
add_executable(session_policy_test
    session_policy_test.cc)
target_link_libraries(session_policy_test PRIVATE smart_home_common pthread)
target_compile_features(session_policy_test PRIVATE cxx_std_11)
target_include_directories(session_policy_test PRIVATE ${SERVER_INCLUDE_DIR} ${COMMON_INCLUDE_DIR})
add_test(NAME session_policy_test COMMAND session_policy_test)
