cmake_minimum_required(VERSION 3.16)
project(server_tests LANGUAGES CXX)

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

#[[录像文件写入器 + 目录检查（角色 A）：不依赖 MySQL / log4cpp / epoll，自动加入 CTest。]]
add_executable(recorder_test
    recorder_test.cc ../src/recorder.cc)
target_link_libraries(recorder_test PRIVATE pthread)
target_compile_features(recorder_test PRIVATE cxx_std_11)
target_include_directories(recorder_test PRIVATE ${SERVER_INCLUDE_DIR})
add_test(NAME recorder_test COMMAND recorder_test)

#[[线程池测试（角色 A）：不依赖第三方库，自动加入 CTest。]]
add_executable(thread_pool_test
    thread_pool_test.cc ../src/thread_pool.cc)
target_link_libraries(thread_pool_test PRIVATE pthread)
target_compile_features(thread_pool_test PRIVATE cxx_std_11)
target_include_directories(thread_pool_test PRIVATE ${SERVER_INCLUDE_DIR})
add_test(NAME thread_pool_test COMMAND thread_pool_test)

#[[假媒体源测试（角色 C 的 MockMediaSource，无 FFmpeg 依赖）：自动加入 CTest。]]
add_executable(mock_media_source_test
    test_mock_media_source.cc ../src/media/mock_media_source.cc)
target_link_libraries(mock_media_source_test PRIVATE smart_home_common pthread)
target_compile_features(mock_media_source_test PRIVATE cxx_std_11)
target_include_directories(mock_media_source_test PRIVATE ${SERVER_INCLUDE_DIR} ${COMMON_INCLUDE_DIR})
add_test(NAME mock_media_source_test COMMAND mock_media_source_test)

#[[Reactor 集成测试（角色 A）：连接/未登录拦截/断线/重连/空闲回收/停止。
    不依赖 MySQL 运行（不注入 Auth/Resource handler），但链接需要完整 server 依赖。]]
add_executable(reactor_test
    reactor_test.cc
    ../src/reactor.cc
    ../src/connection.cc
    ../src/thread_pool.cc
    ../src/stream_session.cc
    ../src/recorder.cc
    ../src/logger.cc
    ../src/config.cc
    ../src/media/mock_media_source.cc
    ../src/MySQLClient.cc
    ../src/UserService.cc
    ../src/AuthHandler.cc
    ../src/DeviceService.cc
    ../src/RecordService.cc
    ../src/ResourceHandler.cc
)
target_link_libraries(reactor_test PRIVATE
    smart_home_common ${MYSQLCLIENT_LIBRARY} OpenSSL::Crypto log4cpp pthread)
target_compile_features(reactor_test PRIVATE cxx_std_11)
target_include_directories(reactor_test PRIVATE ${SERVER_INCLUDE_DIR} ${COMMON_INCLUDE_DIR})
add_test(NAME reactor_test COMMAND reactor_test)
