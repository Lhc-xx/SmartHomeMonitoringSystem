cmake_minimum_required(VERSION 3.16)
project(server_tests LANGUAGES CXX)

add_executable(server_test
    Test.cc
    test_main.cc
)

target_link_libraries(server_test PRIVATE smart_home_common)
target_compile_features(server_test PRIVATE cxx_std_11)
target_include_directories(server_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
)

add_test(NAME server_test COMMAND server_test)
