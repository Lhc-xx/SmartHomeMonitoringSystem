cmake_minimum_required(VERSION 3.16)
project(common LANGUAGES CXX)

#[[
公共静态库集中放置跨平台 TLV 编解码，服务端和客户端共享同一套字节契约。
]]
add_library(smart_home_common STATIC
    # 保留仓库原有公共模块占位源文件，避免本地与服务器构建清单产生差异。
    src/common_placeholder.cc
    src/Protocol.cc

    # 认证业务 value 编解码实现。
    # Server 的 AuthHandler 通过 smart_home_common 复用该实现，
    # 不在 Server target 中重复编译公共协议源码。

    src/AuthProtocol.cc
     # 媒体包序列化
    src/media_packet.cc
    src/DeviceProtocol.cc
    src/RecordProtocol.cc
)

#[[
对外暴露 protocol 目录，使各端只依赖公共头文件而不复制协议实现。
]]
target_include_directories(smart_home_common PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

#[[
协议库保持 C++11，避免改变项目既有工具链要求。
]]
target_compile_features(smart_home_common PUBLIC cxx_std_11)
