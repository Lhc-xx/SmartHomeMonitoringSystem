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

#[[Connection 非阻塞发送队列测试：socketpair 模拟慢客户端，验证部分写与 EPOLLOUT 状态。]]
add_executable(connection_output_test
    connection_output_test.cc ../src/connection.cc)
target_link_libraries(connection_output_test PRIVATE smart_home_common pthread)
target_compile_features(connection_output_test PRIVATE cxx_std_11)
target_include_directories(connection_output_test PRIVATE ${SERVER_INCLUDE_DIR} ${COMMON_INCLUDE_DIR})
add_test(NAME connection_output_test COMMAND connection_output_test)

#[[PTZ 输入白名单测试：只验证非法 URL/动作被拒绝，不访问真实摄像头。]]
add_executable(ptz_handler_test
    ptz_handler_test.cc
    ../src/PtzHandler.cc
    ../src/PtzHttpClient.cc
    ../src/logger.cc)
target_link_libraries(ptz_handler_test PRIVATE
    smart_home_common OpenSSL::Crypto log4cpp pthread
    ${CURL_LIBRARY} ${CJSON_LIBRARY})
target_compile_features(ptz_handler_test PRIVATE cxx_std_11)
target_include_directories(ptz_handler_test PRIVATE
    ${SERVER_INCLUDE_DIR} ${COMMON_INCLUDE_DIR}
    ${CURL_INCLUDE_DIR} ${CJSON_INCLUDE_DIR})
add_test(NAME ptz_handler_test COMMAND ptz_handler_test)

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

#[[TsRecorder 启动握手测试：不访问摄像头，验证 ffmpeg exec 失败会正确返回 false。]]
add_executable(ts_recorder_test
    ts_recorder_test.cc ../src/TsRecorder.cc)
target_link_libraries(ts_recorder_test PRIVATE pthread)
target_compile_features(ts_recorder_test PRIVATE cxx_std_11)
target_include_directories(ts_recorder_test PRIVATE ${SERVER_INCLUDE_DIR})
add_test(NAME ts_recorder_test COMMAND ts_recorder_test)

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
    ../src/media/stream_session.cc
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
    ../src/PtzHandler.cc
    ../src/PtzHttpClient.cc
    ../src/TsRecorder.cc
)
target_link_libraries(reactor_test PRIVATE
    smart_home_common ${MYSQLCLIENT_LIBRARY} OpenSSL::Crypto log4cpp pthread
    ${CURL_LIBRARY} ${CJSON_LIBRARY})
target_compile_features(reactor_test PRIVATE cxx_std_11)
target_include_directories(reactor_test PRIVATE
    ${SERVER_INCLUDE_DIR} ${COMMON_INCLUDE_DIR} ${CURL_INCLUDE_DIR} ${CJSON_INCLUDE_DIR})
add_test(NAME reactor_test COMMAND reactor_test)

#[[流会话测试（角色 C）：Mock 源 + 统一版 StreamSession，无 FFmpeg 依赖，自动加入 CTest。]]
add_executable(stream_session_test
    test_stream_session.cc
    ../src/media/mock_media_source.cc
    ../src/media/stream_session.cc
    ../src/recorder.cc
)
target_link_libraries(stream_session_test PRIVATE smart_home_common pthread)
target_compile_features(stream_session_test PRIVATE cxx_std_11)
target_include_directories(stream_session_test PRIVATE ${SERVER_INCLUDE_DIR} ${COMMON_INCLUDE_DIR})
add_test(NAME stream_session_test COMMAND stream_session_test)

#[[录像切片测试（角色 C）：TsSegmenter，无 FFmpeg 依赖，自动加入 CTest。]]
add_executable(ts_segmenter_test
    test_ts_segmenter.cc
    ../src/media/mock_media_source.cc
    ../src/media/ts_segmenter.cc
)
target_link_libraries(ts_segmenter_test PRIVATE smart_home_common pthread)
target_compile_features(ts_segmenter_test PRIVATE cxx_std_11)
target_include_directories(ts_segmenter_test PRIVATE ${SERVER_INCLUDE_DIR} ${COMMON_INCLUDE_DIR})
add_test(NAME ts_segmenter_test COMMAND ts_segmenter_test)

#[[端到端集成测试（角色 C）：Mock + FFmpeg + 重组器。需 FFmpeg 开发库，未安装则跳过。]]
find_path(FFMPEG_INCLUDE_DIR libavformat/avformat.h)
find_library(AVFORMAT_LIBRARY avformat)
find_library(AVCODEC_LIBRARY avcodec)
find_library(AVUTIL_LIBRARY avutil)
if(FFMPEG_INCLUDE_DIR AND AVFORMAT_LIBRARY AND AVCODEC_LIBRARY AND AVUTIL_LIBRARY)
    add_executable(integration_test
        test_integration.cc
        ../../common/src/media_reassembler.cc
        ../src/media/mock_media_source.cc
        ../src/media/ffmpeg_media_source.cc
        ../src/media/stream_session.cc
        ../src/recorder.cc
    )
    target_link_libraries(integration_test PRIVATE
        smart_home_common ${AVFORMAT_LIBRARY} ${AVCODEC_LIBRARY} ${AVUTIL_LIBRARY} pthread)
    target_compile_features(integration_test PRIVATE cxx_std_11)
    target_include_directories(integration_test PRIVATE
        ${SERVER_INCLUDE_DIR} ${COMMON_INCLUDE_DIR} ${FFMPEG_INCLUDE_DIR})
    add_test(NAME integration_test COMMAND integration_test)
    set_tests_properties(integration_test PROPERTIES
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/../..")
else()
    message(WARNING "FFmpeg dev libraries not found, skipping integration_test")
endif()

