cmake_minimum_required(VERSION 3.16)
project(SmartHomeClient LANGUAGES CXX)

#[[
Qt 的元对象、Designer UI 和资源文件由 CMake 自动处理，保持 Qt Creator 直接打开可构建。
]]
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt5 REQUIRED COMPONENTS Widgets Network Test)
include(CTest)

add_executable(SmartHomeClient
    main.cpp
    MainWindow.cpp
    MainWindow.h
    MainWindow.ui
    network/TcpClient.cpp
    network/TcpClient.h
    protocol/ClientProtocol.cpp
    protocol/ClientProtocol.h
    service/UserService.cpp
    service/UserService.h
    model/DeviceModel.cpp
    model/DeviceModel.h
    model/RecordModel.cpp
    model/RecordModel.h
    ui/LoginWidget.cpp
    ui/LoginWidget.h
    ui/LoginWidget.ui
    video/VideoWidget.cpp
    video/VideoWidget.h
)

#[[
客户端只复用 common 的 ErrorCode/MessageType 定义，协议字节编解码由客户端适配层负责。
]]
target_include_directories(SmartHomeClient PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../common/include
)
target_link_libraries(SmartHomeClient PRIVATE Qt5::Widgets Qt5::Network)
target_compile_features(SmartHomeClient PRIVATE cxx_std_11)

#[[
协议测试只使用 QByteArray，不创建窗口、不连接服务器，符合离线测试约束。
]]
if(BUILD_TESTING)
    add_executable(ClientProtocolTest
        tests/ClientProtocolTest.cpp
        protocol/ClientProtocol.cpp
        protocol/ClientProtocol.h
    )
    target_include_directories(ClientProtocolTest PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/../../../common/include
    )
    target_link_libraries(ClientProtocolTest PRIVATE Qt5::Network)
    target_compile_features(ClientProtocolTest PRIVATE cxx_std_11)
    add_test(NAME ClientProtocolTest COMMAND ClientProtocolTest)

    add_executable(NetworkBehaviorTest
        tests/NetworkBehaviorTest.cpp
        network/TcpClient.cpp
        network/TcpClient.h
        protocol/ClientProtocol.cpp
        protocol/ClientProtocol.h
        service/UserService.cpp
        service/UserService.h
    )
    target_include_directories(NetworkBehaviorTest PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/../../../common/include
    )
    target_link_libraries(NetworkBehaviorTest PRIVATE Qt5::Core Qt5::Network Qt5::Test)
    target_compile_features(NetworkBehaviorTest PRIVATE cxx_std_11)
    add_test(NAME NetworkBehaviorTest COMMAND NetworkBehaviorTest)

    #[[
    服务层测试只使用本机临时 QTcpServer 驱动异步状态机，
    不启动项目服务器，也不会连接或修改真实数据库。
    ]]
    add_executable(UserServiceTest
        tests/UserServiceTest.cpp
        network/TcpClient.cpp
        network/TcpClient.h
        protocol/ClientProtocol.cpp
        protocol/ClientProtocol.h
        service/UserService.cpp
        service/UserService.h
    )
    target_include_directories(UserServiceTest PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/../../../common/include
    )
    target_link_libraries(UserServiceTest PRIVATE Qt5::Network Qt5::Test)
    target_compile_features(UserServiceTest PRIVATE cxx_std_11)
    add_test(NAME UserServiceTest COMMAND UserServiceTest)

    #[[
    主窗口测试只检查 B 数据页控件和交互状态；通过环境变量将连接目标
    固定为本机无服务端口，确保测试不会访问远程服务器或摄像头。
    ]]
    add_executable(MainWindowTest
        tests/MainWindowTest.cpp
        MainWindow.cpp
        MainWindow.h
        MainWindow.ui
        network/TcpClient.cpp
        network/TcpClient.h
        protocol/ClientProtocol.cpp
        protocol/ClientProtocol.h
        service/UserService.cpp
        service/UserService.h
        model/DeviceModel.cpp
        model/DeviceModel.h
        model/RecordModel.cpp
        model/RecordModel.h
        ui/LoginWidget.cpp
        ui/LoginWidget.h
        ui/LoginWidget.ui
    )
    target_include_directories(MainWindowTest PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/../../../common/include
    )
    target_link_libraries(MainWindowTest PRIVATE Qt5::Widgets Qt5::Network Qt5::Test)
    target_compile_features(MainWindowTest PRIVATE cxx_std_11)
    add_test(NAME MainWindowTest COMMAND MainWindowTest)
endif()
