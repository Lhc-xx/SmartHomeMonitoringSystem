#ifndef REACTOR_H
#define REACTOR_H

#include "connection.h"
#include "thread_pool.h"
#include "protocol/MessageType.h"

#include <string>
#include <map>
#include <memory>
#include <atomic>
#include <mutex>

namespace smart_home {
    class AuthHandler;
    class PtzHandler;
    class RecordService;
    class ResourceHandler;
    class TsRecorder;
    namespace media { class StreamSession; } // 流会话存储（媒体转发核心）
    class Reactor{
    public:
        Reactor(size_t thread_num = 4, size_t capacity = 10000);
        ~Reactor();
        bool init(const std::string& ip, int port); // 初始化epoll实例
        void run(); // 事件循环
        void stop(); //退出事件循环
        void setAuthHandler(AuthHandler* handler);
        void setResourceHandler(ResourceHandler* handler);
        void setPtzHandler(PtzHandler* handler); // 云台转发处理器，由 main 注入
        void setRecordService(RecordService* service); // 录像元数据写入，由 main 注入
        void setSessionTimeout(int seconds); // 登录会话超时（秒），<=0 永不超时
        void setVideoPath(const std::string &path); // 录像文件保存目录
        void setIdleTimeout(int seconds); // 连接空闲回收超时（秒），<=0 永不回收

    private:
        void closeConnection(int fd); // 从epoll删除 清理断开连接
        void updateWriteInterest(int fd, bool enabled); // 切换连接的 EPOLLOUT 关注位
        bool finalizeRecording(int fd); // 停止录像、落盘文件并写入 records 元数据
        AuthHandler* _authHandler = nullptr;   // 认证处理器，由 main 注入
        ResourceHandler* _resourceHandler = nullptr; // 资源处理器，由 main 注入
        PtzHandler* _ptzHandler = nullptr;    // 云台转发处理器，由 main 注入
        RecordService* _recordService = nullptr; // 录像元数据写入，由 main 注入
        std::map<int, std::shared_ptr<media::StreamSession>> _streams;   // fd -> 流会话
        std::map<int, std::string> _streamUrls;                          // fd -> 推流 URL（录像用）
        std::map<int, std::shared_ptr<TsRecorder>> _recorders;           // fd -> TS 录像器
        std::map<int, uint64_t> _recordDeviceIds;                        // fd -> 录像 deviceId
        std::map<int, std::string> _recordStartTimes;                    // fd -> 录像开始时间
        std::mutex _streamsMutex;                                 // 保护上面各 map
        void handleMessage(std::shared_ptr<Connection> conn, const TlvMessage &msg);
        void checkIdleConnections();   // 扫描并回收空闲连接
        void sendUnauthorized(std::shared_ptr<Connection> conn, const TlvMessage &msg,
                              MessageType requestType); // 未登录/会话失效时返回 UNAUTHORIZED

    private:
        int _sessionTimeout = 1800; // 登录会话超时（秒）
        std::string _videoPath = "./data/"; // 录像文件保存目录
        int _idleTimeout = 60; // 连接空闲回收超时（秒）
        int _epFd; // epoll 实例fd
        int _listenFd; // 监听的fd
        std::atomic<bool> _runFlag; // 运行标志
        std::map<int, std::shared_ptr<Connection>> _conn; // 连接对象
        ThreadPool _pool; // 线程池, 分发任务
   }; 
}

#endif //  REACTOR_H
