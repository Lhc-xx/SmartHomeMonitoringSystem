#include <cstddef>
#include <cstdint>
#include <string>
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
#include <vector>

#include "protocol/Protocol.h"     // TlvMessage / TlvProtocol / PROTOCOL_VERSION
#include "protocol/MessageType.h"  // MessageType 枚举

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
    TlvMessage msg;
    msg.type = static_cast<uint16_t>(MessageType::REGISTER_REQUEST); // 0x1001
    msg.version = PROTOCOL_VERSION;
    msg.requestId = 1;
    std::string u = "lhc";
    msg.value.assign(u.begin(), u.end());

    auto packet = TlvProtocol::encode(msg); // 12 + 3 = 15字节

    size_t half = packet.size() / 2; // 7字节
    send(sockfd, packet.data(), half, 0);
    sleep(1);
    send(sockfd, packet.data() + half, packet.size() - half, 0); // 发后一半

    // 尝试收服务器响应
    std::vector<uint8_t> rbuf(1024);
    ssize_t n = recv(sockfd, rbuf.data(), rbuf.size(), 0);
    if(n > 0){
        rbuf.resize(n);
        TlvMessage resp;
        if(TlvProtocol::tryDecode(rbuf, resp)){
            std::cout << "response type=" << resp.type << std::endl;   // 期望 4098
            if(resp.value.size() >= 4){
                int32_t code = 0;
                memcpy(&code, resp.value.data(), 4);   // body 前 4 字节
                code = ntohl(code);                    // 网络序转回主机序
                std::cout << "error code=" << code << std::endl;        // 期望 0
            }
        }
    }else if (n == 0) {
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
