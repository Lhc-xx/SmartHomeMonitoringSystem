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
#include <functional>

namespace smart_home{
    class Connection{
    public:
        explicit Connection(int fd); // 唯一性  禁止隐式转换
        ~Connection();
        int fd() const; // 返回_fd对象
        ssize_t readData();
        std::vector<uint8_t> &readBuffer();
        bool readMessage(TlvMessage &msg); // 从缓冲区拆出一个完整包
        /*
         * 将数据追加到线程安全的发送队列，返回实际接收进入队列的字节数。
         * 网络线程通过 flushOutput() 执行真正的 send，避免工作线程阻塞在
         * 慢客户端上，也避免多个业务线程交叉写坏 TLV/媒体帧边界。
         */
        size_t sendData(const std::vector<uint8_t> &data);

        /* Reactor 用该回调切换 EPOLLOUT；回调只做 epoll_ctl，不访问本对象。 */
        void setWriteInterestCallback(const std::function<void(bool)> &callback);

        enum class FlushResult {
            Drained,  // 当前发送队列已经全部写入内核
            Pending,  // 内核发送缓冲已满，等待下一次 EPOLLOUT
            Fatal     // 连接已不可写，需要由 Reactor 统一回收
        };

        /* 在 Reactor 线程中尽可能 flush，支持部分写和 EAGAIN。 */
        FlushResult flushOutput();

        /* 关闭底层 fd；幂等，供 Reactor 在所有断开路径统一调用。 */
        void closeConnection();
        bool isClosed() const;
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
        mutable std::mutex _sendMutex;   // 保护发送队列、fd 和写兴趣状态
        std::vector<uint8_t> _writeBuf; // 工作线程追加、Reactor 线程 flush 的发送队列
        size_t _writeOffset = 0; // 当前队列中已写入内核的字节数
        bool _closed = false;    // closeConnection/fatal send 后禁止继续入队
        bool _writeInterest = false; // 是否已经通知 Reactor 关注 EPOLLOUT
        std::function<void(bool)> _writeInterestCallback;

        // ---- 连接级登录状态机字段 ----
        uint64_t _userId = 0;    // 0 表示未登录
        std::string _token;      // 会话 token（明文，仅内存）
        time_t _loginTime = 0;   // 登录成功时间
        
    };
} 

#endif // CONNECTION_H
