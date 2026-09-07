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

#[[
可选的 VLC（libvlc）播放后端，由 player-xia 工程合并而来。
默认关闭：仓库只提交了 VLC 公共头文件（third_party/vlc/include），
libvlc.lib / libvlccore.lib 及对应 DLL 因体积和 .gitignore 规则未入库。
开启前请先安装 VLC SDK 或完整 VLC，并用 -DVLC_LIBRARY_DIR=<sdk/lib> 指定库目录。
]]
option(WITH_VLC "Build with optional VLC (libvlc) playback backend" OFF)

#[[
可选的「服务器转发 → FFmpeg 解码」后端。默认关闭：Windows 下通常没有 MinGW
的 FFmpeg 开发库，关闭时用 MockDecoder 跑通「重组 → 解码 → 显示」链路；
要真解码需安装 FFmpeg 开发库并用 -DWITH_QT_FFMPEG=ON 开启。
]]
option(WITH_QT_FFMPEG "Build Qt client with real FFmpeg (avcodec/swscale) decoder" OFF)

set(FFMPEG_DECODER_SOURCES "")
set(FFMPEG_DECODER_LIBRARIES "")
set(QT_FFMPEG_INCLUDE_DIR "")
if(WITH_QT_FFMPEG)
    find_path(QT_FFMPEG_INCLUDE_DIR libavcodec/avcodec.h)
    find_library(QT_AVCODEC_LIBRARY avcodec)
    find_library(QT_AVUTIL_LIBRARY avutil)
    find_library(QT_SWSCALE_LIBRARY swscale)
    if(QT_FFMPEG_INCLUDE_DIR AND QT_AVCODEC_LIBRARY AND QT_AVUTIL_LIBRARY AND QT_SWSCALE_LIBRARY)
        list(APPEND FFMPEG_DECODER_SOURCES ../src/ffmpeg_decoder.cc)
        list(APPEND FFMPEG_DECODER_LIBRARIES ${QT_AVCODEC_LIBRARY} ${QT_AVUTIL_LIBRARY} ${QT_SWSCALE_LIBRARY})
        message(STATUS "FFmpeg found, building FFmpegDecoder into SmartHomeClient")
    else()
        message(WARNING "FFmpeg dev libraries not found; falling back to MockDecoder")
    endif()
endif()

add_executable(SmartHomeClient
    main.cpp
    MainWindow.cpp
    MainWindow.h
    MainWindow.ui
    network/TcpClient.cpp
    network/TcpClient.h
    network/PtzClient.cpp
    network/PtzClient.h
    protocol/ClientProtocol.cpp
    protocol/ClientProtocol.h
    service/UserService.cpp
    service/UserService.h
    model/DeviceModel.cpp
    model/DeviceModel.h
    model/RecordModel.cpp
    model/RecordModel.h
    video/CameraConfig.cpp
    video/CameraConfig.h
    video/RtspPlayer.cpp
    video/RtspPlayer.h
    ui/LoginWidget.cpp
    ui/LoginWidget.h
    ui/LoginWidget.ui
    ui/RegisterDialog.cpp
    ui/RegisterDialog.h
    ui/RegisterDialog.ui
    ui/MonitoringDashboard.cpp
    ui/MonitoringDashboard.h
    video/VideoWidget.cpp
    video/VideoWidget.h
    video/ServerStreamPlayer.cpp
    video/ServerStreamPlayer.h
    video/FilePlaybackPlayer.cpp
    video/FilePlaybackPlayer.h
    ../src/mock_decoder.cc
    ${FFMPEG_DECODER_SOURCES}
    ../../../common/src/media_packet.cc
    ../../../common/src/media_reassembler.cc
)

#[[
客户端只复用 common 的 ErrorCode/MessageType 定义，协议字节编解码由客户端适配层负责。
]]
target_include_directories(SmartHomeClient PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../common/include
)
target_link_libraries(SmartHomeClient PRIVATE Qt5::Widgets Qt5::Network)
target_compile_features(SmartHomeClient PRIVATE cxx_std_11)

# FFmpeg 解码器（角色 C，检测到时编译并链接）
if(FFMPEG_DECODER_SOURCES)
    target_include_directories(SmartHomeClient PRIVATE ${QT_FFMPEG_INCLUDE_DIR})
    target_link_libraries(SmartHomeClient PRIVATE ${FFMPEG_DECODER_LIBRARIES})
    target_compile_definitions(SmartHomeClient PRIVATE SMART_HOME_WITH_FFMPEG=1)
endif()

if(WITH_VLC)
    set(VLC_LIBRARY_DIR "" CACHE PATH "Directory containing libvlc(.lib/.so) and libvlccore(.lib/.so)")
    find_library(VLC_LIBRARY
        NAMES libvlc vlc
        HINTS ${VLC_LIBRARY_DIR}
        PATH_SUFFIXES lib sdk/lib)
    find_library(VLCCORE_LIBRARY
        NAMES libvlccore vlccore
        HINTS ${VLC_LIBRARY_DIR}
        PATH_SUFFIXES lib sdk/lib)
    if(NOT VLC_LIBRARY OR NOT VLCCORE_LIBRARY)
        message(FATAL_ERROR
            "WITH_VLC=ON 但未找到 libvlc/libvlccore 库，请用 -DVLC_LIBRARY_DIR=<sdk/lib 目录> 指定。")
    endif()

    target_sources(SmartHomeClient PRIVATE
        video/VlcPlayer.cpp
        video/VlcPlayer.h
    )
    target_include_directories(SmartHomeClient PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party/vlc/include
    )
    target_link_libraries(SmartHomeClient PRIVATE ${VLC_LIBRARY} ${VLCCORE_LIBRARY})
    target_compile_definitions(SmartHomeClient PRIVATE SMART_HOME_WITH_VLC=1)
    #[[运行时需要 libvlc.dll / libvlccore.dll 及 plugins 目录与可执行文件同级。]]
endif()

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

    add_executable(CameraConfigTest
        tests/CameraConfigTest.cpp
        video/CameraConfig.cpp
        video/CameraConfig.h
    )
    target_include_directories(CameraConfigTest PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
    )
    target_link_libraries(CameraConfigTest PRIVATE Qt5::Core Qt5::Test)
    target_compile_features(CameraConfigTest PRIVATE cxx_std_11)
    add_test(NAME CameraConfigTest COMMAND CameraConfigTest)

    add_executable(RtspPlayerTest
        tests/RtspPlayerTest.cpp
        video/RtspPlayer.cpp
        video/RtspPlayer.h
    )
    target_include_directories(RtspPlayerTest PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
    )
    target_link_libraries(RtspPlayerTest PRIVATE Qt5::Core Qt5::Gui Qt5::Network Qt5::Test)
    target_compile_features(RtspPlayerTest PRIVATE cxx_std_11)
    add_test(NAME RtspPlayerTest COMMAND RtspPlayerTest)

    add_executable(FilePlaybackPlayerTest
        tests/FilePlaybackPlayerTest.cpp
        video/FilePlaybackPlayer.cpp
        video/FilePlaybackPlayer.h
        video/RtspPlayer.cpp
        video/RtspPlayer.h
    )
    target_include_directories(FilePlaybackPlayerTest PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
    )
    target_link_libraries(FilePlaybackPlayerTest PRIVATE Qt5::Core Qt5::Gui Qt5::Network Qt5::Test)
    target_compile_features(FilePlaybackPlayerTest PRIVATE cxx_std_11)
    add_test(NAME FilePlaybackPlayerTest COMMAND FilePlaybackPlayerTest)

    add_executable(VideoWidgetTest
        tests/VideoWidgetTest.cpp
        video/VideoWidget.cpp
        video/VideoWidget.h
    )
    target_include_directories(VideoWidgetTest PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
    )
    target_link_libraries(VideoWidgetTest PRIVATE Qt5::Widgets Qt5::Test)
    target_compile_features(VideoWidgetTest PRIVATE cxx_std_11)
    add_test(NAME VideoWidgetTest COMMAND VideoWidgetTest)

    add_executable(PtzClientTest
        tests/PtzClientTest.cpp
        network/PtzClient.cpp
        network/PtzClient.h
    )
    target_include_directories(PtzClientTest PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
    )
    target_link_libraries(PtzClientTest PRIVATE Qt5::Core Qt5::Network Qt5::Test)
    target_compile_features(PtzClientTest PRIVATE cxx_std_11)
    add_test(NAME PtzClientTest COMMAND PtzClientTest)

    add_executable(MonitoringDashboardTest
        tests/MonitoringDashboardTest.cpp
        ui/MonitoringDashboard.cpp
        ui/MonitoringDashboard.h
        video/CameraConfig.cpp
        video/CameraConfig.h
        video/RtspPlayer.cpp
        video/RtspPlayer.h
        video/VideoWidget.cpp
        video/VideoWidget.h
        network/PtzClient.cpp
        network/PtzClient.h
    )
    target_include_directories(MonitoringDashboardTest PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/../../../common/include
    )
    target_link_libraries(MonitoringDashboardTest PRIVATE Qt5::Widgets Qt5::Network Qt5::Test)
    target_compile_features(MonitoringDashboardTest PRIVATE cxx_std_11)
    add_test(NAME MonitoringDashboardTest COMMAND MonitoringDashboardTest)

    add_executable(NetworkBehaviorTest
        tests/NetworkBehaviorTest.cpp
        network/TcpClient.cpp
        network/TcpClient.h
        protocol/ClientProtocol.cpp
        protocol/ClientProtocol.h
        service/UserService.cpp
        service/UserService.h
        ../../../common/src/media_packet.cc
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
        ../../../common/src/media_packet.cc
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
        ui/RegisterDialog.cpp
        ui/RegisterDialog.h
        ui/RegisterDialog.ui
        ui/MonitoringDashboard.cpp
        ui/MonitoringDashboard.h
        video/CameraConfig.cpp
        video/CameraConfig.h
        video/RtspPlayer.cpp
        video/RtspPlayer.h
        video/VideoWidget.cpp
        video/VideoWidget.h
        video/ServerStreamPlayer.cpp
        video/ServerStreamPlayer.h
        video/FilePlaybackPlayer.cpp
        video/FilePlaybackPlayer.h
        ../src/mock_decoder.cc
        ../../../common/src/media_packet.cc
        ../../../common/src/media_reassembler.cc
        network/PtzClient.cpp
        network/PtzClient.h
    )
    target_include_directories(MainWindowTest PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/../include
        ${CMAKE_CURRENT_SOURCE_DIR}/../../../common/include
    )
    target_link_libraries(MainWindowTest PRIVATE Qt5::Widgets Qt5::Network Qt5::Test)
    target_compile_features(MainWindowTest PRIVATE cxx_std_11)
    add_test(NAME MainWindowTest COMMAND MainWindowTest)

    add_executable(RegisterDialogTest
        tests/RegisterDialogTest.cpp
        ui/RegisterDialog.cpp
        ui/RegisterDialog.h
        ui/RegisterDialog.ui
        ui/LoginWidget.cpp
        ui/LoginWidget.h
        ui/LoginWidget.ui
        network/TcpClient.cpp
        network/TcpClient.h
        protocol/ClientProtocol.cpp
        protocol/ClientProtocol.h
        service/UserService.cpp
        service/UserService.h
        ../../../common/src/media_packet.cc
    )
    target_include_directories(RegisterDialogTest PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/../../../common/include
    )
    target_link_libraries(RegisterDialogTest PRIVATE Qt5::Widgets Qt5::Network Qt5::Test)
    target_compile_features(RegisterDialogTest PRIVATE cxx_std_11)
    add_test(NAME RegisterDialogTest COMMAND RegisterDialogTest)
endif()
