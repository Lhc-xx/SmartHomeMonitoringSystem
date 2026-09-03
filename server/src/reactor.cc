#include "reactor.h"

#include <cerrno>
#include <sys/epoll.h>
#include <unistd.h>
#include <iostream>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <cstring>


namespace smart_home {
    Reactor::Reactor()
    : _epFd(-1)
    , _listenFd(-1)
    , _runFlag(false)
    {
        _epFd = epoll_create1(0);
        if(_epFd < 0){
            std::cerr << "Reactor() failed" << std::endl;
        }
    }

    Reactor::~Reactor(){
        if(_listenFd >= 0){
            close(_listenFd);
        }
        if(_epFd >= 0){
            close(_epFd);
        }
    }

    bool Reactor::init(const std::string &ip, int port){
        // 1.创建socket对象  listenfd实例
        _listenFd = socket(AF_INET, SOCK_STREAM, 0);
        if(_listenFd < 0){
            return false;
        }

        // ip 复用
        int opt = 1;
        setsockopt(_listenFd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

        // 2.bind
        struct sockaddr_in addr{};
        // 主机字节序  转  网络字节序
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = inet_addr(ip.c_str());
        addr.sin_port = htons(port);
        if(bind(_listenFd, (struct sockaddr *)&addr, sizeof(addr)) < 0){ 
            return false; // 绑定失败
        }

        // 3.listen
        if(listen(_listenFd, 128) < 0){
            return false;
        }

        // 4.监听fd注册到epoll
        struct epoll_event ev;
        ev.events = EPOLLIN; // 关心读事件
        ev.data.fd = _listenFd; // fd塞进用户数据
        if(epoll_ctl(_epFd, EPOLL_CTL_ADD, _listenFd, &ev) < 0){
            return false;
        }
        return true;
    }

    void Reactor::run() {
        // read/recv  write/send
        _runFlag = true;
        struct epoll_event events[64]; // 就绪事件列表
        while (_runFlag) {
            // n为就绪事件个数
            int n = epoll_wait(_epFd, events, 64, -1);
            if(n < 0 && errno == EINTR){
                continue; // 信号被中断打断  继续
            }
            if(n < 0){
                break;
            }
            // 正常
            for(int i = 0; i < n; ++i){
                int fd = events[i].data.fd; 
                if(fd == _listenFd){
                    // fd为监听的fd  == 有新连接来了
                    int connFd = accept4(_listenFd, nullptr, nullptr, SOCK_NONBLOCK);
                    if(connFd < 0){
                        continue;
                    }
                    std::cout << "new connection, fd: " << connFd << std::endl;
                    close(connFd);

                }else{
                    // 已连接的fd  有读写/断开事件发生
                }

            }
        }
        
    }
    void Reactor::stop() {
        // close
        _runFlag = false;
    }
}