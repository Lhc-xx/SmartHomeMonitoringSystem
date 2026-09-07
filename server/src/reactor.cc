#include "reactor.h"
#include "connection.h"
#include "logger.h"
#include "protocol/Protocol.h"
#include "protocol/ErrorCode.h"
#include "protocol/MessageType.h"
#include "protocol/AuthProtocol.h"
#include "session_policy.h"
#include "AuthHandler.h"
#include "ResourceHandler.h"
#include "PtzHandler.h"
#include "TsRecorder.h"
#include "media/stream_session.h"
#include "media/mock_media_source.h"
#ifdef SMARTHOME_WITH_FFMPEG
#include "media/ffmpeg_media_source.h"
#endif

#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <ctime>
#include <string>
#include <sys/epoll.h>
#include <sys/types.h>
#include <unistd.h>
#include <iostream>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <cstring>
#include <algorithm>
#include <map>
#include <vector>

namespace smart_home {

namespace {

// STREAM_START 请求体：uint16(大端) 长度 + UTF-8 字节的 stream URL。
// 长度字段不足 / 数据不完整 / 长度为 0 时返回空串（表示使用默认 Mock 源）。
std::string parseStreamUrl(const std::vector<uint8_t> &value) {
    if (value.size() < 2) {
        return std::string();
    }
    const uint16_t len = static_cast<uint16_t>((value[0] << 8) | value[1]);
    if (value.size() < static_cast<size_t>(2) + len) {
        return std::string();
    }
    return std::string(reinterpret_cast<const char *>(value.data() + 2), len);
}

// 根据 stream URL 选择媒体源：空串或 "mock://" 前缀用 Mock，其余交给 FFmpeg。
// 未编译 FFmpeg 时始终回退 Mock，保证服务器在无 FFmpeg 环境下仍可构建、可演示。
std::unique_ptr<media::MediaSource> makeMediaSource(const std::string &url) {
#ifdef SMARTHOME_WITH_FFMPEG
    if (!url.empty() && url.compare(0, 7, "mock://") != 0) {
        return std::unique_ptr<media::MediaSource>(new media::FFmpegMediaSource());
    }
#else
    (void)url;
#endif
    return std::unique_ptr<media::MediaSource>(new media::MockMediaSource());
}

}  // namespace

    Reactor::Reactor(size_t thread_num, size_t capacity)
    : _epFd(-1)
    , _listenFd(-1)
    , _runFlag(false)
    , _pool(thread_num, capacity) // 初始化线程池
    {
        _epFd = epoll_create1(0);
        if(_epFd < 0){
            std::cerr << "Reactor() failed" << std::endl;
        }
    }

    Reactor::~Reactor(){
        if(_listenFd >= 0){
            close(_listenFd);
        }
        if(_epFd >= 0){
            close(_epFd);
        }
    }

    bool Reactor::init(const std::string &ip, int port){
        // 1.创建socket对象  listenfd实例
        _listenFd = socket(AF_INET, SOCK_STREAM, 0);
        if(_listenFd < 0){
            return false;
        }

        // ip 复用
        int opt = 1;
        setsockopt(_listenFd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

        // 2.bind
        struct sockaddr_in addr{};
        // 主机字节序  转  网络字节序
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = inet_addr(ip.c_str());
        addr.sin_port = htons(port);
        if(bind(_listenFd, (struct sockaddr *)&addr, sizeof(addr)) < 0){ 
            return false; // 绑定失败
        }

        // 3.listen
        if(listen(_listenFd, 128) < 0){
            return false;
        }

        // 4.监听fd注册到epoll
        struct epoll_event ev;
        ev.events = EPOLLIN; // 关心读事件
        ev.data.fd = _listenFd; // fd塞进用户数据
        if(epoll_ctl(_epFd, EPOLL_CTL_ADD, _listenFd, &ev) < 0){
            return false;
        }
        return true;
    }

    void Reactor::run() {
        // read/recv  write/send
        _runFlag = true;
        struct epoll_event events[64]; // 就绪事件列表
        while (_runFlag) {
            // n为就绪事件个数
            int n = epoll_wait(_epFd, events, 64, 1000); // 1s超时
            if(n < 0 && errno == EINTR){
                continue; // 信号被中断打断  继续
            }
            if(n < 0){
                break;
            }
            if(n == 0){   // 超时唤醒，没有就绪事件，扫描空闲连接
                checkIdleConnections();
                continue;
            }
            // 正常
            for(int i = 0; i < n; ++i){
                int fd = events[i].data.fd; 
                if(fd == _listenFd){
                    // fd为监听的fd  == 有新连接来了
                    int connFd = accept4(_listenFd, nullptr, nullptr, SOCK_NONBLOCK);
                    if(connFd < 0){
                        continue;
                    }
                    auto conn = std::make_shared<Connection>(connFd);
                    _conn[connFd] = conn; // 连接信息存入map
                    struct epoll_event ev;
                    ev.events = EPOLLIN; // 监视可读事件
                    ev.data.fd = connFd;
                    epoll_ctl(_epFd, EPOLL_CTL_ADD, connFd, &ev); // 注册进epoll
                    LOG_INFO(("new connection, fd = " + std::to_string(connFd)).c_str());
                }else{
                    // 已连接的fd  有读写/断开事件发生
                    auto it = _conn.find(fd);
                    if(it == _conn.end()){
                        continue; // 未找到连接 跳过
                    }
                    auto conn = it->second;

                    ssize_t n = conn->readData();
                    if(n > 0){ // 有数据
                        TlvMessage msg;
                        while (conn->readMessage(msg)) {
                            LOG_INFO(("recv tlv: type=" + std::to_string(msg.type)
                                    + " body_len=" + std::to_string(msg.value.size())).c_str());

                            _pool.addTask([this, conn, msg](){
                                handleMessage(conn, msg);
                            });
                        }
                    }else if(n == 0){
                        // 对端关闭
                        LOG_INFO(("connection close, fd = " + std::to_string(fd)).c_str());
                        closeConnection(fd);
                    }else{
                        // n < 0 出错
                        if(errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR){
                            // 无数据 忽略
                        }else{
                            closeConnection(fd);
                        }
                    }
                }

            }
        }
        
    }

    void Reactor::stop() {
        // close
        _runFlag = false;
    }

    void Reactor::closeConnection(int fd){
        epoll_ctl(_epFd, EPOLL_CTL_DEL, fd, nullptr);

        // 停止并回收该连接的流会话 / 录像器
        {
            std::lock_guard<std::mutex> guard(_streamsMutex);
            auto it = _streams.find(fd);
            if(it != _streams.end()){
                it->second->stop();
                _streams.erase(it);
            }
            _streamUrls.erase(fd);
            auto rit = _recorders.find(fd);
            if(rit != _recorders.end()){
                rit->second->stop();
                _recorders.erase(rit);
            }
        }

        auto it = _conn.find(fd);
        if(it != _conn.end()){
            _conn.erase(it);
        }
    }

    void Reactor::checkIdleConnections(){
        if(_idleTimeout <= 0){
            return; // 永不回收空闲连接
        }
        time_t now = time(nullptr);
        std::vector<int> idleFds;
        for(auto &kv : _conn){
            if(now - kv.second->lastActive() >= _idleTimeout){
                idleFds.push_back(kv.first);
            }
        }
        for(int fd : idleFds){
            LOG_INFO(("idle timeout, close fd=" + std::to_string(fd)).c_str());
            closeConnection(fd);
        }
    }

    void Reactor::setAuthHandler(AuthHandler* handler){
        _authHandler = handler;
    }

    void Reactor::setResourceHandler(ResourceHandler* handler){
        _resourceHandler = handler;
    }

    void Reactor::setPtzHandler(PtzHandler* handler){
        _ptzHandler = handler;
    }

    void Reactor::setSessionTimeout(int seconds){
        _sessionTimeout = seconds;
    }

    void Reactor::setVideoPath(const std::string &path){
        _videoPath = path.empty() ? "./data/" : path;
    }

    void Reactor::setIdleTimeout(int seconds){
        _idleTimeout = seconds;
    }

    // 未登录 / 会话失效时，按请求类型返回对应的 UNAUTHORIZED 响应。
    void Reactor::sendUnauthorized(std::shared_ptr<Connection> conn,
                                   const TlvMessage &msg, MessageType requestType){
        TlvMessage resp;
        resp.version   = PROTOCOL_VERSION;
        resp.requestId = msg.requestId;
        resp.type      = responseTypeFor(requestType);
        if (!buildUnauthorizedValue(requestType, resp.value)) {
            return; // 非受保护请求，不在拦截范围
        }
        conn->sendData(TlvProtocol::encode(resp));
    }

    void Reactor::handleMessage(std::shared_ptr<Connection> conn, const TlvMessage &msg){
        const MessageType type = static_cast<MessageType>(msg.type);

        // 注册：无需登录即可访问。
        if (type == MessageType::REGISTER_REQUEST) {
            if (_authHandler) {
                TlvMessage resp = _authHandler->handle(msg);
                conn->sendData(TlvProtocol::encode(resp));
            }
            return;
        }

        // 登录：成功后把会话信息写入连接状态，作为后续请求的鉴权依据。
        if (type == MessageType::LOGIN_REQUEST) {
            if (_authHandler) {
                TlvMessage resp = _authHandler->handle(msg);
                uint64_t userId = 0;
                std::string token;
                ErrorCode code = ErrorCode::INTERNAL_ERROR;
                if (AuthProtocol::decodeLoginResponse(resp.value, userId, token, code)
                        && code == ErrorCode::SUCCESS) {
                    conn->markLoggedIn(userId, token);
                    LOG_INFO(("login success, fd=" + std::to_string(conn->fd())
                              + " user=" + std::to_string(userId)).c_str());
                }
                conn->sendData(TlvProtocol::encode(resp));
            }
            return;
        }

        // 需要登录的消息：设备列表 / 录像查询 / 推流 / 停流 —— 未登录拦截。
        if (requiresAuth(type)) {
            const time_t now = time(nullptr);
            // 会话超时：登录态存在但已超过会话超时时间 → 强制登出。
            if (conn->isAuthenticated() && conn->isSessionExpired(now, _sessionTimeout)) {
                LOG_INFO(("session expired, logout fd=" + std::to_string(conn->fd())).c_str());
                conn->markLoggedOut();
            }
            // 未登录拦截：连接未登录时拒绝所有需要登录的请求。
            if (!conn->isAuthenticated()) {
                sendUnauthorized(conn, msg, type);
                return;
            }
        }

        // 资源请求（设备列表/录像查询）：交给 B 的 ResourceHandler（内部再做 token 会话校验）。
        if (type == MessageType::DEVICE_LIST_REQUEST ||
            type == MessageType::RECORD_QUERY_REQUEST) {
            if (_resourceHandler) {
                TlvMessage resp = _resourceHandler->handle(msg);
                conn->sendData(TlvProtocol::encode(resp));
            }
            return;
        }

        // 云台控制：交给 D 的 PtzHandler（内部经 libcurl+token 转发到摄像头）。
        if (type == MessageType::PTZ_CONTROL_REQUEST) {
            if (_ptzHandler) {
                TlvMessage resp = _ptzHandler->handle(msg);
                conn->sendData(TlvProtocol::encode(resp));
            }
            return;
        }

        // 流媒体/录像控制请求：按 stream URL 选择 Mock 或 FFmpeg 源（见 makeMediaSource）。
        TlvMessage resp;
        resp.version   = PROTOCOL_VERSION;
        resp.requestId = msg.requestId;

        int32_t errCode = static_cast<int32_t>(ErrorCode::SUCCESS);

        switch (type) {
            case MessageType::STREAM_START_REQUEST:
                resp.type = static_cast<uint16_t>(MessageType::STREAM_START_RESPONSE);
                {
                    // 按请求携带的 stream URL 选择源：空 / mock:// → Mock；否则 FFmpeg。
                    const std::string url = parseStreamUrl(msg.value);
                    std::unique_ptr<media::MediaSource> source = makeMediaSource(url);
                    auto session = std::make_shared<media::StreamSession>(std::move(source));
                    session->setSink([conn](const std::vector<uint8_t> &bytes) { conn->sendData(bytes); });
                    const std::string openUrl = url.empty() ? "mock://test" : url;
                    if (!session->start(openUrl)) {
                        errCode = static_cast<int32_t>(ErrorCode::STREAM_OPEN_FAILED);
                        LOG_WARN(("stream open failed, fd=" + std::to_string(conn->fd())
                                  + " url=" + openUrl).c_str());
                    } else {
                        LOG_INFO(("stream start, fd=" + std::to_string(conn->fd())
                                  + " url=" + openUrl).c_str());
                        std::lock_guard<std::mutex> guard(_streamsMutex);
                        _streams[conn->fd()] = session;
                        _streamUrls[conn->fd()] = openUrl;
                    }
                }
                break;

            case MessageType::STREAM_STOP_REQUEST:
                resp.type = static_cast<uint16_t>(MessageType::STREAM_STOP_RESPONSE);
                {
                    std::lock_guard<std::mutex> guard(_streamsMutex);
                    auto it = _streams.find(conn->fd());
                    if (it != _streams.end()) {
                        it->second->stop();
                        _streams.erase(it);
                    }
                    _streamUrls.erase(conn->fd());
                    auto rit = _recorders.find(conn->fd());
                    if (rit != _recorders.end()) {
                        rit->second->stop();
                        _recorders.erase(rit);
                    }
                }
                break;

            case MessageType::RECORD_START_REQUEST:
                resp.type = static_cast<uint16_t>(MessageType::RECORD_START_RESPONSE);
                {
                    // 解析 deviceId（8 字节大端 uint64），用于生成录像目录名
                    uint64_t deviceId = 0;
                    if (msg.value.size() >= 8) {
                        for (size_t i = 0; i < 8; ++i) {
                            deviceId = (deviceId << 8) | msg.value[i];
                        }
                    }
                    std::string url;
                    {
                        std::lock_guard<std::mutex> guard(_streamsMutex);
                        auto it = _streamUrls.find(conn->fd());
                        if (it != _streamUrls.end()) {
                            url = it->second;
                        }
                        if (_recorders.find(conn->fd()) != _recorders.end()) {
                            errCode = static_cast<int32_t>(ErrorCode::RECORD_ALREADY_STARTED);
                        }
                    }
                    if (errCode == static_cast<int32_t>(ErrorCode::RECORD_ALREADY_STARTED)) {
                        // 已在录制
                    } else if (url.empty() || url == "mock://test") {
                        errCode = static_cast<int32_t>(ErrorCode::STREAM_NOT_FOUND);
                    } else {
                        auto recorder = std::make_shared<TsRecorder>();
                        const std::string outDir = _videoPath + "/" + std::to_string(deviceId)
                                                 + "_" + std::to_string(time(nullptr));
                        if (!recorder->start(url, outDir, 10)) {
                            errCode = static_cast<int32_t>(ErrorCode::RECORD_OPEN_FAILED);
                        } else {
                            std::lock_guard<std::mutex> guard(_streamsMutex);
                            _recorders[conn->fd()] = recorder;
                            LOG_INFO(("record start, fd=" + std::to_string(conn->fd())
                                      + " dir=" + outDir + " url=" + url).c_str());
                        }
                    }
                }
                break;

            case MessageType::RECORD_STOP_REQUEST:
                resp.type = static_cast<uint16_t>(MessageType::RECORD_STOP_RESPONSE);
                {
                    std::shared_ptr<TsRecorder> recorder;
                    {
                        std::lock_guard<std::mutex> guard(_streamsMutex);
                        auto it = _recorders.find(conn->fd());
                        if (it != _recorders.end()) {
                            recorder = it->second;
                            _recorders.erase(it);
                        }
                    }
                    if (!recorder) {
                        errCode = static_cast<int32_t>(ErrorCode::RECORD_NOT_STARTED);
                    } else {
                        recorder->stop();
                        const std::vector<std::string> files = recorder->producedFiles();
                        LOG_INFO(("record stop, fd=" + std::to_string(conn->fd())
                                  + " segments=" + std::to_string(files.size())).c_str());
                    }
                }
                break;

            default:
                resp.type = msg.type;
                errCode   = static_cast<int32_t>(ErrorCode::UNKNOWN_MESSAGE);
                LOG_WARN(("unknown message type: " + std::to_string(msg.type)).c_str());
                break;
        }

        int32_t code = htonl(errCode);
        uint8_t *p = reinterpret_cast<uint8_t *>(&code);
        resp.value.assign(p, p + 4);

        conn->sendData(TlvProtocol::encode(resp));
    }
}
