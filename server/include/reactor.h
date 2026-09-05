#ifndef REACTOR_H
#define REACTOR_H

#include "connection.h"
#include "thread_pool.h"

#include <string>
#include <map>

namespace smart_home {
    class AuthHandler;
    class Reactor{
    public:
        Reactor(size_t thread_num = 4, size_t capacity = 10000);
        ~Reactor();
        bool init(const std::string& ip, int port); // 初始化epoll实例
        void run(); // 事件循环
        void stop(); //退出事件循环
        void setAuthHandler(AuthHandler* handler);

    private:
        void closeConnection(int fd); // 从epoll删除 清理断开连接
        AuthHandler* _authHandler = nullptr;   // 认证处理器，由 main 注入

    private:
        int _epFd; // epoll 实例fd
        int _listenFd; // 监听的fd
        bool _runFlag; // 运行标志
        std::map<int, Connection*> _conn; // 连接对象
        ThreadPool _pool; // 线程池, 分发任务
   }; 
}

#endif //  REACTOR_H