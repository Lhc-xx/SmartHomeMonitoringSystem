cmake_minimum_required(VERSION 3.16)
project(common_tests LANGUAGES CXX)

add_executable(common_test
    test_main.cc
)

target_link_libraries(common_test PRIVATE smart_home_common)
target_compile_features(common_test PRIVATE cxx_std_11)
add_test(NAME common_test COMMAND common_test)

# ==========================================
# AuthProtocol协议测试
# ==========================================

add_executable(
    auth_protocol_test

    auth_protocol_test.cc
)


target_link_libraries(
    auth_protocol_test

    PRIVATE

    smart_home_common
)


target_compile_features(
    auth_protocol_test

    PRIVATE

    cxx_std_11
)


target_include_directories(
    auth_protocol_test

    PRIVATE

    ${CMAKE_CURRENT_SOURCE_DIR}/../include
)


add_test(
    NAME auth_protocol_test
    COMMAND auth_protocol_test
)