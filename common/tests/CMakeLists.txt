cmake_minimum_required(VERSION 3.16)
project(common_tests LANGUAGES CXX)

#[[公共协议测试只依赖 smart_home_common，不需要服务器、数据库或真实网络。]]
set(COMMON_TEST_TARGETS
    common_test
    protocol_test
    auth_protocol_test
    device_protocol_test
    record_protocol_test
)

add_executable(common_test test_main.cc)
add_executable(protocol_test protocol_test.cc)
add_executable(auth_protocol_test auth_protocol_test.cc)
add_executable(device_protocol_test device_protocol_test.cc)
add_executable(record_protocol_test record_protocol_test.cc)

foreach(test_target IN LISTS COMMON_TEST_TARGETS)
    target_link_libraries(${test_target} PRIVATE smart_home_common)
    target_compile_features(${test_target} PRIVATE cxx_std_11)
endforeach()

add_test(NAME common_test COMMAND common_test)
add_test(NAME protocol_test COMMAND protocol_test)
add_test(NAME auth_protocol_test COMMAND auth_protocol_test)
add_test(NAME device_protocol_test COMMAND device_protocol_test)
add_test(NAME record_protocol_test COMMAND record_protocol_test)
