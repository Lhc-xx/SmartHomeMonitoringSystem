#include "reactor.h"
#include "connection.h"

#include <cerrno>
#include <string>
#include <sys/epoll.h>
#include <sys/types.h>
#include <unistd.h>
#include <iostream>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <cstring>


namespace smart_home {
    Reactor::Reactor(size_t thread_num, size_t capacity)
    : _epFd(-1)
    , _listenFd(-1)
    , _runFlag(false)
    , _pool(thread_num, capacity) // 初始化线程池
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

        for(auto& kv: _conn){
            delete kv.second;
        }
        _conn.clear();
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
                    Connection * conn = new Connection(connFd);
                    _conn[connFd] = conn; // 连接信息存入map
                    struct epoll_event ev;
                    ev.events = EPOLLIN; // 监视可读事件
                    ev.data.fd = connFd;
                    epoll_ctl(_epFd, EPOLL_CTL_ADD, connFd, &ev); // 注册进epoll
                    std::cout << "new connection, fd: " << connFd << std::endl;

                }else{
                    // 已连接的fd  有读写/断开事件发生
                    auto it = _conn.find(fd);
                    if(it == _conn.end()){
                        continue; // 未找到连接 跳过
                    }
                    Connection* conn = it->second;

                    ssize_t n = conn->readData();
                    if(n > 0){ // 有数据
                        std::string data = conn->readBuffer();
                        conn->readBuffer().clear();
                        _pool.addTask([data](){
                            std::cout << "recv " << data.size() << " bytes: "<< data << std:: endl;
                        });
                    }else if(n == 0){
                        // 对端关闭
                        std::cout << "connection close, fd = " << fd << std:: endl;
                        closeConnection(fd);
                    }else{
                        // n < 0 出错
                        if(errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR){
                            // 无数据 忽略
                        }else{
                            closeConnection(fd);
                        }
                    }
                }

            }
        }
        
    }

    void Reactor::stop() {
        // close
        _runFlag = false;
    }

    void Reactor::closeConnection(int fd){
        epoll_ctl(_epFd, EPOLL_CTL_DEL, fd, nullptr); // 从epoll监视名单移除fd
        auto it = _conn.find(fd);
        if(it != _conn.end()){
            delete it->second;
            _conn.erase(it); // 从map中也会删除fd连接信息
        }
    }
}