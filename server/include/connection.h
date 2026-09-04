#ifndef CONNECTION_H
#define CONNECTION_H

#include "protocol/Protocol.h"

#include <cstddef>
#include <vector>
#include <cstdint>
#include <sys/types.h>

namespace smart_home{
    class Connection{
    public:
        explicit Connection(int fd); // 唯一性  禁止隐式转换
        ~Connection();
        int fd() const; // 返回_fd对象
        ssize_t readData();
        std::vector<uint8_t> &readBuffer();
        bool readMessage(TlvMessage &msg); // 从话冲去拆出一个完整包
        size_t sendData(const std::vector<uint8_t> &data); // 发送数据

    private:
        int _fd; // 连接fd
        std::vector<uint8_t> _readBuf; // 读缓冲区
        
    };
} 

#endif // CONNECTION_H