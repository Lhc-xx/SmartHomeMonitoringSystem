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

#include "client_config.h"         // 客户端配置读取（角色 A）

#include "protocol/Protocol.h"     // TlvMessage / TlvProtocol / PROTOCOL_VERSION
#include "protocol/MessageType.h"  // MessageType 枚举
#include "protocol/AuthProtocol.h"   // ← B 的认证协议（封包/解包）
#include "protocol/ErrorCode.h"      // ← 错误码枚举
#include "protocol/media_packet.h"   // MediaPacket / MediaPacketSerializer

int main(int argc, char *argv[]) {
    // 0. 读取客户端配置（默认 client/linux/conf/client.conf，可用命令行参数覆盖）
    std::string config_path = "client/linux/conf/client.conf";
    if (argc > 1) {
        config_path = argv[1];
    }
    smart_home::ClientConfig cfg;
    if (!smart_home::loadClientConfig(config_path, cfg)) {
        std::cout << "[warn] cannot load config " << config_path
                  << ", using defaults" << std::endl;
    }
    std::cout << "connect to " << cfg.server_ip << ":" << cfg.server_port << std::endl;

    // 1.socket
    int sockfd;
    // 2.connect
    struct sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr(cfg.server_ip.c_str());
    addr.sin_port = htons(cfg.server_port);
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
        // ---- 请求推流 ----
    TlvMessage req;
    req.type    = static_cast<uint16_t>(MessageType::STREAM_START_REQUEST); // 0x1401
    req.version = PROTOCOL_VERSION;
    req.requestId = 1;

    auto reqBuf = TlvProtocol::encode(req);
    send(sockfd, reqBuf.data(), reqBuf.size(), 0);

    // ---- 接收：先解析响应，再统计媒体帧 ----
    std::vector<uint8_t> buf;
    bool gotResp = false;
    int frames = 0;

    while(frames < 20){
        char tmp[4096];
        ssize_t n = recv(sockfd, tmp, sizeof(tmp), 0);
        if(n <= 0){
            break;   // 超时或断开
        }
        buf.insert(buf.end(), tmp, tmp + n);

        // 先尝试解析 TLV 响应（可能和媒体帧混在一起）
        if(!gotResp){
            std::vector<uint8_t> work = buf;
            TlvMessage resp;
            if(TlvProtocol::tryDecode(work, resp)){
                gotResp = true;
                std::cout << "stream start resp: type=" << resp.type << std::endl;
                buf.swap(work);
            }
        }

        // 再拆媒体帧
        while(true){
            uint32_t fl = 0;
            if(!smart_home::protocol::MediaPacketSerializer::peekFrameLength(buf.data(), buf.size(), fl)){
                break;
            }
            if(buf.size() < fl){
                break;
            }
            smart_home::protocol::MediaPacket pkt;
            if(smart_home::protocol::MediaPacketSerializer::decode(buf.data(), fl, pkt) == 0){
                break;
            }
            frames++;
            if(frames <= 3){
                std::cout << "frame " << frames << ": pts=" << pkt.pts
                          << " size=" << pkt.data.size()
                          << (pkt.isKeyFrame() ? " [KEY]" : "") << std::endl;
            }
            buf.erase(buf.begin(), buf.begin() + fl);
        }
    }
    std::cout << "received " << frames << " media frames" << std::endl;
    
    // 4.close
    close(sockfd);
    return 0;
}
