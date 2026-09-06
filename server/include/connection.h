#ifndef CONNECTION_H
#define CONNECTION_H

#include "protocol/Protocol.h"

#include <cstddef>
#include <vector>
#include <cstdint>
#include <sys/types.h>
#include <mutex>
#include <ctime>   // time_t / time()
#include <string>  // std::string（会话 token）

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
        void updateLastActive();        // 刷新最后活跃时间
        time_t lastActive() const;      // 读最后活跃时间

        // ---- 连接级登录状态机（角色 A）----
        void markLoggedIn(uint64_t userId, const std::string &token); // 登录成功写入会话
        void markLoggedOut();             // 登出/会话失效，清空登录态
        bool isAuthenticated() const;     // 是否已登录（userId != 0）
        uint64_t userId() const;          // 登录用户 id（未登录 0）
        const std::string &token() const; // 会话 token（明文，仅内存）
        time_t loginTime() const;         // 登录时间戳（未登录 0）
        bool isSessionExpired(time_t now, int timeoutSeconds) const; // 会话是否超时

    private:
        time_t _lastActive;             // 最后活跃时间
    
    private:
        int _fd; // 连接fd
        std::vector<uint8_t> _readBuf; // 读缓冲区
        std::mutex _sendMutex;   // 保护 sendData 的并发写

        // ---- 连接级登录状态机字段 ----
        uint64_t _userId = 0;    // 0 表示未登录
        std::string _token;      // 会话 token（明文，仅内存）
        time_t _loginTime = 0;   // 登录成功时间
        
    };
} 

#endif // CONNECTION_H