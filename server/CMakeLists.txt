cmake_minimum_required(VERSION 3.16)

project(server LANGUAGES CXX)

# ============================================================
# FFmpeg（角色 C：真拉流源，可选）
#
# 检测到 libavformat/libavcodec/libavutil 时，把 FFmpegMediaSource
# 编入服务器主程序，并用 SMARTHOME_WITH_FFMPEG 宏开启 reactor.cc 里
# 「STREAM_START 按 stream URL 选择真源」的逻辑。
# 未安装 FFmpeg 开发库时回退为 Mock 源，服务器仍可正常构建。
# ============================================================
option(WITH_FFMPEG "Build server with FFmpeg real media source (libavformat)" ON)

set(FFMPEG_MEDIA_SOURCES "")
set(FFMPEG_LIBRARIES "")
# 注意：不要用 set(FFMPEG_INCLUDE_DIR "") 预置 find_path 的结果变量，
# 否则 find_path 会把已有普通变量当成结果而跳过搜索，导致 FFmpeg 检测失败。
if(WITH_FFMPEG)
    find_path(FFMPEG_INCLUDE_DIR libavformat/avformat.h)
    find_library(AVFORMAT_LIBRARY avformat)
    find_library(AVCODEC_LIBRARY avcodec)
    find_library(AVUTIL_LIBRARY avutil)
    if(FFMPEG_INCLUDE_DIR AND AVFORMAT_LIBRARY AND AVCODEC_LIBRARY AND AVUTIL_LIBRARY)
        list(APPEND FFMPEG_MEDIA_SOURCES src/media/ffmpeg_media_source.cc)
        list(APPEND FFMPEG_LIBRARIES ${AVFORMAT_LIBRARY} ${AVCODEC_LIBRARY} ${AVUTIL_LIBRARY})
        message(STATUS "FFmpeg found, building FFmpegMediaSource into smart_home_server")
    else()
        message(WARNING "FFmpeg dev libraries not found; STREAM_START will fall back to MockMediaSource")
    endif()
endif()

# ============================================================
# libcurl + cJSON（角色 D：云台 HTTP 转发 + JSON 解析）
# ============================================================
find_path(CURL_INCLUDE_DIR curl/curl.h)
find_library(CURL_LIBRARY curl)
find_path(CJSON_INCLUDE_DIR cjson/cJSON.h)
find_library(CJSON_LIBRARY cjson)
if(NOT CURL_INCLUDE_DIR OR NOT CURL_LIBRARY OR NOT CJSON_INCLUDE_DIR OR NOT CJSON_LIBRARY)
    message(FATAL_ERROR "libcurl/cJSON not found (needed by PtzHandler)")
endif()

add_executable(smart_home_server
    src/main.cc # 主程序入口
    src/logger.cc # 日志模块
    src/config.cc # 配置模块
    src/thread_pool.cc # 线程池模块
    src/reactor.cc # 网络模块
    src/stream_request.cc # 流/录像请求体严格解析（纯逻辑边界）
    src/connection.cc # 连接模块
    src/media/stream_session.cc #流会话模块（媒体转发核心）
    src/recorder.cc # 录像文件写入器（角色 A：录像文件生命周期）
    src/TsRecorder.cc # TS 录像器（角色 C：ffmpeg CLI 分段录 MPEG-TS）

    src/media/mock_media_source.cc # 假媒体源（测试用，不依赖 FFmpeg）
    ${FFMPEG_MEDIA_SOURCES} # FFmpeg 真拉流源（角色 C，检测到 FFmpeg 时编入）

    # --------------------------------------------------------
    # B：MySQL 数据访问层
    # --------------------------------------------------------
    src/MySQLClient.cc
    src/UserService.cc
    src/AuthHandler.cc
    src/DeviceService.cc
    src/RecordService.cc
    src/ResourceHandler.cc

    # --------------------------------------------------------
    # D：云台 HTTP 转发（libcurl + cJSON + token）
    # --------------------------------------------------------
    src/PtzHttpClient.cc
    src/PtzHandler.cc
)

# ============================================================
# 查找 OpenSSL
#
# UserService 中密码安全处理使用：
#
# RAND_bytes()
# PKCS5_PBKDF2_HMAC()
# EVP_sha256()
#
# 这些实现位于 libcrypto 中。
# ============================================================

find_package(
    OpenSSL REQUIRED
)


# ============================================================
# 查找 MySQL Client 库
#
# MySQLClient 中使用：
#
# mysql_init()
# mysql_real_connect()
# mysql_real_query()
# mysql_close()
# ============================================================

find_library(
    MYSQLCLIENT_LIBRARY
    mysqlclient
)


if(NOT MYSQLCLIENT_LIBRARY)

    message(
        FATAL_ERROR
        "libmysqlclient not found"
    )

endif()

target_link_libraries(
    smart_home_server

    PRIVATE

    # 公共 TLV 协议模块
    smart_home_common


    # 日志
    log4cpp


    # std::thread / pthread
    pthread


    # MySQL C API
    ${MYSQLCLIENT_LIBRARY}


    # UserService 密码哈希、随机 salt
    OpenSSL::Crypto

    # FFmpeg 真拉流源（角色 C，检测到时非空）
    ${FFMPEG_LIBRARIES}

    # 云台 HTTP 转发（角色 D）
    ${CURL_LIBRARY}
    ${CJSON_LIBRARY}
)


# ============================================================
# 使用 C++11
# ============================================================

target_compile_features(
    smart_home_server

    PRIVATE

    cxx_std_11
)


# ============================================================
# Server 头文件目录
#
# 这样源码中可以直接：
#
# #include "config.h"
# #include "MySQLClient.h"
# #include "UserService.h"
# ============================================================

target_include_directories(
    smart_home_server

    PRIVATE

    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CURL_INCLUDE_DIR}
    ${CJSON_INCLUDE_DIR}
)

# FFmpeg 头文件目录 + 编译宏（检测到时生效）
if(FFMPEG_INCLUDE_DIR)
    target_include_directories(smart_home_server PRIVATE ${FFMPEG_INCLUDE_DIR})
endif()
if(FFMPEG_MEDIA_SOURCES)
    target_compile_definitions(smart_home_server PRIVATE SMARTHOME_WITH_FFMPEG=1)
endif()

# 可执行文件输出到项目根目录
set_target_properties(smart_home_server PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}
)
