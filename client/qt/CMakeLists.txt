cmake_minimum_required(VERSION 3.16)
project(qt_client LANGUAGES CXX)

#[[
Qt 客户端实际工程沿用 SmartHomeClient 子目录，便于 Qt Creator 直接打开。
顶层 BUILD_QT_CLIENT 打开时复用同一个目标，避免维护两套源文件清单。
]]
add_subdirectory(SmartHomeClient)
