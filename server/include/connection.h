#ifndef CONNECTION_H
#define CONNECTION_H

#include <string>
#include <sys/types.h>

namespace smart_home{
    class Connection{
    public:
        explicit Connection(int fd); // 唯一性  禁止隐式转换
        ~Connection();
        int fd() const; // 返回_fd对象
        ssize_t readData();
        std::string &readBuffer();
    private:
        int _fd; // 连接fd
        std::string _readBuf; // 读缓冲区
        
    };
} 

#endif // CONNECTION_H