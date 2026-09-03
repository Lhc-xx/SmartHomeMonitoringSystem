cmake_minimum_required(VERSION 3.16)
project(server LANGUAGES CXX)

add_executable(smart_home_server
    src/main.cc # 主程序入口
    src/logger.cc # 日志模块
    src/config.cc # 配置模块
)

target_link_libraries(smart_home_server PRIVATE smart_home_common log4cpp pthread)
target_compile_features(smart_home_server PRIVATE cxx_std_11)
target_include_directories(smart_home_server PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)