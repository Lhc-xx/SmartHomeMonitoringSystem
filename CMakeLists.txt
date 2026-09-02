cmake_minimum_required(VERSION 3.16)
project(SmartHomeMonitoringSystem LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(BUILD_SERVER "Build server target" ON)
option(BUILD_LINUX_CLIENT "Build Linux C++ client placeholder" ON)
option(BUILD_QT_CLIENT "Build Qt client placeholder" OFF)
option(BUILD_TESTS "Build basic test targets" ON)

add_subdirectory(common)

if(BUILD_SERVER)
    add_subdirectory(server)
endif()

if(BUILD_LINUX_CLIENT)
    add_subdirectory(client/linux)
endif()

if(BUILD_QT_CLIENT)
    add_subdirectory(client/qt)
endif()

if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(common/tests)
    add_subdirectory(server/tests)
    add_subdirectory(client/linux/tests)
endif()
