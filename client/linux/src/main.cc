#include <sys/time.h> // struct timeval
#include <cerrno>       
#include <sys/socket.h>   // socket / connect / send / recv
#include <netinet/in.h>   // sockaddr_in、htons
#include <arpa/inet.h>    // inet_addr
#include <sys/types.h>
#include <unistd.h>       // close
#include <cstring>        // strlen
#include <cstdio>       
#include <iostream>       // 打印

int main() {
    // 1.socket
    int sockfd;
    // 2.connect
    struct sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    addr.sin_port = htons(7777);
    while(true){
        sockfd = socket(AF_INET, SOCK_STREAM, 0);
        if(sockfd < 0){
            perror("socket");
            return 1;
        }

        // 2.1 断线重连
        if(connect(sockfd, (struct sockaddr*)&addr, sizeof(addr)) == 0){
            std::cout << "connected!" << std::endl;
            break;   // 连上了，跳出重试循环
        }

        perror("connect failed, retry in 1s");
        close(sockfd);
        sleep(1);
    }

    // 2.2 设置收发超时  收/发各2s
    struct timeval tv;
    tv.tv_sec = 2;
    tv.tv_usec = 0;
    setsockopt(sockfd, SOL_SOCKET, 
        SO_RCVTIMEO, &tv, sizeof(tv)); // 收超时
    setsockopt(sockfd, SOL_SOCKET, 
        SO_SNDTIMEO, &tv, sizeof(tv)); // 发超时

    // 3.send
    const char* msg = "hello from linux client";
    ssize_t s = send(sockfd, msg, strlen(msg), 0);
    if (s < 0) {
        perror("send");
        close(sockfd);
        return 1;
    }

    // 尝试收服务器响应
    char buf[1024];
    ssize_t n = recv(sockfd, buf, sizeof(buf), 0);
    if(n > 0){
        std::cout << "recv " << n << " bytes" << std::endl;
    } else if (n == 0) {
        std::cout << "server closed" << std::endl;
    } else {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            std::cout << "recv timeout" << std::endl;   // 超过 2 秒没数据 → 超时
        } else {
            perror("recv");
        }
    }
    // 4.close
    close(sockfd);
    return 0;
}
