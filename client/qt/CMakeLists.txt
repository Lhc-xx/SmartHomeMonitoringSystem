cmake_minimum_required(VERSION 3.16)
project(qt_client LANGUAGES CXX)

# Qt 5.14.2 接入时在此处补充 find_package(Qt5 COMPONENTS Widgets Network Multimedia REQUIRED)
# 以及 Qt 的源文件、UI 文件和资源文件。
add_custom_target(smart_home_qt_client_placeholder
    COMMENT "Qt client placeholder: configure Qt 5.14.2 before adding GUI sources"
)
