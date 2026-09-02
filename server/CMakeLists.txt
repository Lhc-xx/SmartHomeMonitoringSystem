cmake_minimum_required(VERSION 3.16)
project(server LANGUAGES CXX)

add_executable(smart_home_server
    src/main.cc
)

target_link_libraries(smart_home_server PRIVATE smart_home_common)
target_compile_features(smart_home_server PRIVATE cxx_std_11)
target_include_directories(smart_home_server PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
