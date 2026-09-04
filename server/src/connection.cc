#include "connection.h"
#include "protocol/Protocol.h"

#include <cstddef>
#include <cstdint>
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
        return ::send(_fd, data.data(), data.size(), 0);
    }
}