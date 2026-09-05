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
#include "protocol/AuthProtocol.h"   // ← B 的认证协议（封包/解包）
#include "protocol/ErrorCode.h"      // ← 错误码枚举

// 发送一个请求，接收并解析响应。成功返回 true。
bool sendRequest(int sockfd, const TlvMessage &req, TlvMessage &resp){
    auto buf = TlvProtocol::encode(req);
    send(sockfd, buf.data(), buf.size(), 0);

    std::vector<uint8_t> rbuf(1024);
    ssize_t n = recv(sockfd, rbuf.data(), rbuf.size(), 0);
    if(n <= 0){
        return false;
    }
    rbuf.resize(n);
    return TlvProtocol::tryDecode(rbuf, resp);
}

// 从响应 body 前 4 字节读错误码（网络序转主机序）
int32_t readErrorCode(const TlvMessage &resp){
    int32_t code = 0;
    if(resp.value.size() >= 4){
        memcpy(&code, resp.value.data(), 4);
        code = ntohl(code);
    }
    return code;
}

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
    // 注册请求
    TlvMessage regReq;
    regReq.type    = static_cast<uint16_t>(MessageType::REGISTER_REQUEST); // 0x1001
    regReq.version = PROTOCOL_VERSION;
    regReq.requestId = 1;

    // 用 B 的 AuthProtocol 把用户名/密码封进 body
    std::string username = "lhc";
    std::string password = "123456";
    AuthProtocol::encodeRegisterRequest(username, password, regReq.value);

    TlvMessage regResp;
    if(sendRequest(sockfd, regReq, regResp)){
        ErrorCode code = ErrorCode::INTERNAL_ERROR;
        AuthProtocol::decodeRegisterResponse(regResp.value, code);
        std::cout << "register: type=" << regResp.type
                  << " error=" << static_cast<int32_t>(code) << std::endl;
    }else{
        std::cout << "register failed (no response)" << std::endl;
    }
    
    // 4.close
    close(sockfd);
    return 0;
}
