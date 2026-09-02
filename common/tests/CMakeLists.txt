cmake_minimum_required(VERSION 3.16)
project(common_tests LANGUAGES CXX)

add_executable(common_test
    test_main.cc
)

target_link_libraries(common_test PRIVATE smart_home_common)
target_compile_features(common_test PRIVATE cxx_std_11)
add_test(NAME common_test COMMAND common_test)
