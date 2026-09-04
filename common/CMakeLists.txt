cmake_minimum_required(VERSION 3.16)

project(common LANGUAGES CXX)


add_library(smart_home_common STATIC

    # 协议实现
    src/Protocol.cc

)


# 头文件搜索路径
target_include_directories(
    smart_home_common
    PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)


target_compile_features(
    smart_home_common
    PUBLIC
    cxx_std_11
)