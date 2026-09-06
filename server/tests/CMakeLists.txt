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
