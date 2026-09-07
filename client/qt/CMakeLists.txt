cmake_minimum_required(VERSION 3.16)
project(qt_client LANGUAGES CXX)

#[[
当 Qt 客户端作为仓库根项目的子目录时，测试开关统一由根项目的 BUILD_TESTS 控制。
独立使用本 CMake 文件时，保留 CTest 的 BUILD_TESTING 默认值。
]]
if(NOT CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR AND DEFINED BUILD_TESTS)
    set(BUILD_TESTING ${BUILD_TESTS} CACHE BOOL "Build the testing tree." FORCE)
endif()

include(CTest)

#[[
Qt 客户端实际工程沿用 SmartHomeClient 子目录，便于 Qt Creator 直接打开。
顶层 BUILD_QT_CLIENT 打开时复用同一个目标，避免维护两套源文件清单。
]]
add_subdirectory(SmartHomeClient)
