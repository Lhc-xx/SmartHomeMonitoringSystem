#include "reactor.h"
#include "connection.h"
#include "logger.h"
#include "protocol/Protocol.h"
#include "protocol/ErrorCode.h"
#include "protocol/MessageType.h"
#include "AuthHandler.h"

#include <cerrno>
#include <cstdint>
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
        // Login/register payloads are kept deliberately small and simple:
        // username and password may be separated by NUL, '\n', or ':'.  A
        // username-only registration is accepted for compatibility with the
        // original client; it creates an account with an empty password.
        bool parseCredentials(const std::vector<uint8_t> &body,
                              std::string &username,
                              std::string &password) {
            if (body.empty()) {
                return false;
            }

            std::vector<uint8_t>::const_iterator separator =
                std::find(body.begin(), body.end(), static_cast<uint8_t>(0));
            if (separator == body.end()) {
                separator = std::find(body.begin(), body.end(),
                                      static_cast<uint8_t>('\n'));
            }
            if (separator == body.end()) {
                separator = std::find(body.begin(), body.end(),
                                      static_cast<uint8_t>(':'));
            }

            const std::size_t usernameLength =
                static_cast<std::size_t>(separator - body.begin());
            if (usernameLength == 0 || usernameLength > 64) {
                return false;
            }

            username.assign(body.begin(), body.begin() + usernameLength);
            if (separator != body.end()) {
                password.assign(separator + 1, body.end());
            } else {
                password.clear();
            }

            // Keep the limits in sync with the users table and avoid storing
            // embedded NULs in credentials when a textual separator is used.
            if (password.size() > 256 ||
                std::find(username.begin(), username.end(), '\0') != username.end()) {
                return false;
            }
            return true;
        }

        // Compare credentials without returning early on the first mismatch.
        bool samePassword(const std::string &left, const std::string &right) {
            const std::size_t maxLength = std::max(left.size(), right.size());
            unsigned char difference =
                static_cast<unsigned char>(left.size() != right.size());
            for (std::size_t i = 0; i < maxLength; ++i) {
                const unsigned char l = i < left.size()
                    ? static_cast<unsigned char>(left[i]) : 0;
                const unsigned char r = i < right.size()
                    ? static_cast<unsigned char>(right[i]) : 0;
                difference = static_cast<unsigned char>(difference | (l ^ r));
            }
            return difference == 0;
        }
    }

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
        // The current project has no database client yet.  Keep the account
        // store scoped to this server run so registration and login still
        // form a functional authentication flow without global state.
        std::map<std::string, std::string> users;
        struct epoll_event events[64]; // 就绪事件列表
        while (_runFlag) {
            // n为就绪事件个数
            int n = epoll_wait(_epFd, events, 64, -1);
            if(n < 0 && errno == EINTR){
                continue; // 信号被中断打断  继续
            }
            if(n < 0){
                break;
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
        epoll_ctl(_epFd, EPOLL_CTL_DEL, fd, nullptr); // 从epoll监视名单移除fd
        auto it = _conn.find(fd);
        if(it != _conn.end()){
            _conn.erase(it); // 从map中也会删除fd连接信息
        }
    }

    void Reactor::setAuthHandler(AuthHandler* handler){
        _authHandler = handler;
    }

    void Reactor::handleMessage(std::shared_ptr<Connection> conn, const TlvMessage &msg){
        // 注册请求：交给 B 的 AuthHandler
        if (static_cast<MessageType>(msg.type) == MessageType::REGISTER_REQUEST) {
            if(_authHandler){
                TlvMessage resp = _authHandler->handle(msg);
                conn->sendData(TlvProtocol::encode(resp));
            }
            return;
        }

        // 其它类型：switch stub
        TlvMessage resp;
        resp.version   = PROTOCOL_VERSION;
        resp.requestId = msg.requestId;

        int32_t errCode = static_cast<int32_t>(ErrorCode::SUCCESS);

        switch (static_cast<MessageType>(msg.type)) {
            case MessageType::LOGIN_REQUEST:
                resp.type = static_cast<uint16_t>(MessageType::LOGIN_RESPONSE);
                conn->setAuthenticated(true);
                break;
            case MessageType::DEVICE_LIST_REQUEST:
                resp.type = static_cast<uint16_t>(MessageType::DEVICE_LIST_RESPONSE);
                if (!conn->isAuthenticated()) {
                    errCode = static_cast<int32_t>(ErrorCode::UNAUTHORIZED);
                }
                break;
            case MessageType::RECORD_QUERY_REQUEST:
                resp.type = static_cast<uint16_t>(MessageType::RECORD_QUERY_RESPONSE);
                if (!conn->isAuthenticated()) {
                    errCode = static_cast<int32_t>(ErrorCode::UNAUTHORIZED);
                }
                break;
            default:
                resp.type = msg.type;
                errCode   = static_cast<int32_t>(ErrorCode::UNKNOWN_MESSAGE);
                LOG_WARN(("unknown message type: " + std::to_string(msg.type)).c_str());
                break;
        }

        int32_t code = htonl(errCode);
        uint8_t *p = reinterpret_cast<uint8_t*>(&code);
        resp.value.assign(p, p + 4);

        conn->sendData(TlvProtocol::encode(resp));
    }
}
