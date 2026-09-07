cmake_minimum_required(VERSION 3.16)
project(linux_client LANGUAGES CXX)

add_executable(smart_home_linux_client
    src/main.cc
)

target_link_libraries(smart_home_linux_client PRIVATE smart_home_common)
target_compile_features(smart_home_linux_client PRIVATE cxx_std_11)
target_include_directories(smart_home_linux_client PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)

# 可执行文件输出到项目根目录
set_target_properties(smart_home_linux_client PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}
)
