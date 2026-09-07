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
    : _lastActive(time(nullptr))
    , _fd(fd)
    {

    }

    Connection::~Connection(){
        closeConnection();
    }

    // 返回当前 fd；关闭竞态下返回 -1，调用方可安全放弃本次操作。
    int Connection::fd() const{
        std::lock_guard<std::mutex> guard(_sendMutex);
        return _fd;
    }

    ssize_t Connection::readData(){
        if (_fd < 0) {
            errno = EBADF;
            return -1;
        }
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
        if (data.empty()) {
            return 0;
        }

        std::lock_guard<std::mutex> guard(_sendMutex);
        if (_closed || _fd < 0) {
            return 0;
        }

        /*
         * 业务线程只做内存追加，不在这里调用阻塞/部分 send；否则一个慢客户端
         * 会占住线程池工作线程，并可能让 TLV 响应和媒体帧在多个线程中交错。
         */
        _writeBuf.insert(_writeBuf.end(), data.begin(), data.end());
        if (!_writeInterest) {
            _writeInterest = true;
            if (_writeInterestCallback) {
                /* 回调约定只修改 Reactor 的 epoll 关注位，不回调 Connection。 */
                _writeInterestCallback(true);
            }
        }
        return data.size();
    }

    void Connection::setWriteInterestCallback(
        const std::function<void(bool)> &callback) {
        std::lock_guard<std::mutex> guard(_sendMutex);
        _writeInterestCallback = callback;
        if (!_closed && !_writeBuf.empty() && !_writeInterest) {
            _writeInterest = true;
            if (_writeInterestCallback) {
                _writeInterestCallback(true);
            }
        }
    }

    Connection::FlushResult Connection::flushOutput() {
        std::lock_guard<std::mutex> guard(_sendMutex);
        if (_closed || _fd < 0) {
            return FlushResult::Fatal;
        }

        while (_writeOffset < _writeBuf.size()) {
            const size_t remaining = _writeBuf.size() - _writeOffset;
            const ssize_t written = ::send(
                _fd, _writeBuf.data() + _writeOffset, remaining, MSG_NOSIGNAL);
            if (written > 0) {
                _writeOffset += static_cast<size_t>(written);
                continue;
            }
            if (written < 0 && errno == EINTR) {
                continue;
            }
            if (written < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
                return FlushResult::Pending;
            }

            /* EPIPE/ECONNRESET 等不可恢复错误交由 Reactor 统一 close。 */
            _closed = true;
            _writeBuf.clear();
            _writeOffset = 0;
            _writeInterest = false;
            return FlushResult::Fatal;
        }

        _writeBuf.clear();
        _writeOffset = 0;
        if (_writeInterest) {
            _writeInterest = false;
            if (_writeInterestCallback) {
                _writeInterestCallback(false);
            }
        }
        return FlushResult::Drained;
    }

    void Connection::closeConnection() {
        std::lock_guard<std::mutex> guard(_sendMutex);
        if (_closed && _fd < 0) {
            return;
        }
        _closed = true;
        _writeBuf.clear();
        _writeOffset = 0;
        _writeInterest = false;
        if (_fd >= 0) {
            ::close(_fd);
            _fd = -1;
        }
    }

    bool Connection::isClosed() const {
        std::lock_guard<std::mutex> guard(_sendMutex);
        return _closed;
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
