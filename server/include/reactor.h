#ifndef REACTOR_H
#define REACTOR_H

#include "connection.h"
#include "thread_pool.h"

#include <string>
#include <map>

namespace smart_home {
    class Reactor{
    public:
        Reactor();
        ~Reactor();
        bool init(const std::string& ip, int port); // 初始化epoll实例
        void run(); // 事件循环
        void stop(); //退出事件循环

    private:
        void closeConnection(int fd); // 从epoll删除 清理断开连接

    private:
        int _epFd; // epoll 实例fd
        int _listenFd; // 监听的fd
        bool _runFlag; // 运行标志
        std::map<int, Connection*> _conn; // 连接对象
        ThreadPool _pool; // 线程池, 分发任务
   }; 
}

#endif //  REACTOR_H