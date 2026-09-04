cmake_minimum_required(VERSION 3.16)
project(common LANGUAGES CXX)

add_library(smart_home_common STATIC
    src/common_placeholder.cc
)

target_include_directories(smart_home_common PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_features(smart_home_common PUBLIC cxx_std_11)
