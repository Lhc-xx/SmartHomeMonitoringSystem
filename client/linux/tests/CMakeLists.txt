cmake_minimum_required(VERSION 3.16)
project(linux_client_tests LANGUAGES CXX)

add_executable(linux_client_test
    test_main.cc
)

target_link_libraries(linux_client_test PRIVATE smart_home_common)
target_compile_features(linux_client_test PRIVATE cxx_std_11)
add_test(NAME linux_client_test COMMAND linux_client_test)
