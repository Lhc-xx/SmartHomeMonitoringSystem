#include "connection.h"
#include "protocol/Protocol.h"

#include <cstddef>
#include <cstdint>
#include <ctime>
#include <string>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <cerrno>
#include <iostream>
#include <vector>

namespace smart_home{
    Connection::Connection(int fd)
    : _fd(fd)
    , _lastActive(time(nullptr))
    {

    }

    Connection::~Connection(){
        if(_fd >= 0){
            close(_fd);
        }
    }

    // 返回_fd
    int Connection::fd() const{
        return _fd;
    }

    ssize_t Connection::readData(){
        char buf[4096]; // 临时缓冲区 一次最多都4096个字节
        ssize_t n = ::read(_fd, buf, sizeof(buf)); // 从socket读
        if(n > 0){
            _readBuf.insert(_readBuf.end(), buf, buf + n);
            updateLastActive();   // 有数据就刷新
        }
        return n; // >0 正常数据; 0 对端关闭; -1出错或暂时无数据
    }

    std::vector<uint8_t>& Connection::readBuffer(){
        return _readBuf;
    }

    bool Connection::readMessage(TlvMessage &msg){
        return TlvProtocol::tryDecode(_readBuf, msg);
    }

    size_t Connection::sendData(const std::vector<uint8_t> &data){
        std::lock_guard<std::mutex> guard(_sendMutex);
        return ::send(_fd, data.data(), data.size(), 0);
    }


    void Connection::updateLastActive(){
        _lastActive = time(nullptr);
    }

    time_t Connection::lastActive() const{
        return _lastActive;
    }

    // ---- 连接级登录状态机（角色 A）----
    void Connection::markLoggedIn(uint64_t userId, const std::string &token){
        _userId = userId;
        _token = token;
        _loginTime = time(nullptr);
    }

    void Connection::markLoggedOut(){
        _userId = 0;
        _token.clear();
        _loginTime = 0;
    }

    bool Connection::isAuthenticated() const{
        return _userId != 0;
    }

    uint64_t Connection::userId() const{
        return _userId;
    }

    const std::string &Connection::token() const{
        return _token;
    }

    time_t Connection::loginTime() const{
        return _loginTime;
    }

    bool Connection::isSessionExpired(time_t now, int timeoutSeconds) const{
        if(_userId == 0 || _loginTime == 0){
            return false;   // 未登录，谈不上会话过期
        }
        if(timeoutSeconds <= 0){
            return false;   // 超时时间非正数视为永不超时
        }
        return (now - _loginTime) >= timeoutSeconds;
    }
}