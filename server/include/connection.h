#ifndef CONNECTION_H
#define CONNECTION_H

#include "protocol/Protocol.h"

#include <cstddef>
#include <vector>
#include <cstdint>
#include <sys/types.h>
#include <mutex>
#include <atomic>
#include <ctime>   // time_t / time()

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
        bool isAuthenticated() const;      // 是否已登录
        void setAuthenticated(bool v);     // 设置登录状态
        void updateLastActive();        // 刷新最后活跃时间
        time_t lastActive() const;      // 读最后活跃时间

    private:
        std::atomic<bool> _authenticated{false};       // 默认未登录
        time_t _lastActive;             // 最后活跃时间
    
    private:
        int _fd; // 连接fd
        std::vector<uint8_t> _readBuf; // 读缓冲区
        std::mutex _sendMutex;   // 保护 sendData 的并发写
        
    };
} 

#endif // CONNECTION_H