#include "connection.h"

#include <string>
#include <sys/types.h>
#include <unistd.h>
#include <cerrno>
#include <iostream>

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
            _readBuf.append(buf, n);
        }
        return n; // >0 正常数据; 0 对端关闭; -1出错或暂时无数据
    }

    std::string& Connection::readBuffer(){
        return _readBuf;
    }
}