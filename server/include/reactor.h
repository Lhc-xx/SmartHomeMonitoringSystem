#ifndef REACTOR_H
#define REACTOR_H

#include <string>

namespace smart_home {
    class Reactor{
    private:
        int _epFd; // epoll 实例fd
        int _listenFd; // 监听的fd
        bool _runFlag; // 运行标志
    
    public:
        Reactor();
        ~Reactor();
        bool init(const std::string& ip, int port); // 初始化epoll实例
        void run(); // 事件循环
        void stop(); //退出事件循环
        
   }; 
}

#endif //  REACTOR_H