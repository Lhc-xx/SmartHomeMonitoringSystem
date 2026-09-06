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

// TlvMessage / TlvProtocol / MessageType / PROTOCOL_VERSION 位于全局命名空间。
using smart_home::protocol::MediaPacket;
using smart_home::protocol::MediaPacketSerializer;

// 构造 8 字节大端 uint64（RECORD_START 的 deviceId 字段）。
static std::vector<uint8_t> makeU64BE(uint64_t v) {
    std::vector<uint8_t> out(8, 0);
    for (int i = 0; i < 8; ++i) {
        out[i] = static_cast<uint8_t>((v >> ((7 - i) * 8)) & 0xFF);
    }
    return out;
}

// 发送一个 TLV 请求。
static void sendRequest(int fd, uint16_t type, const std::vector<uint8_t>& value,
                        uint32_t requestId) {
    TlvMessage req;
    req.type = type;
    req.version = PROTOCOL_VERSION;
    req.requestId = requestId;
    req.value = value;
    std::vector<uint8_t> buf = TlvProtocol::encode(req);
    if (!buf.empty()) {
        send(fd, buf.data(), buf.size(), 0);
    }
}

// 从混合流里消费一个条目：
//   先判断是否为媒体帧（peekFrameLength 校验帧长 + 魔数，不会误吞 TLV），
//   否则尝试 TLV 解码。返回 1=TLV(msg)、2=媒体帧(pkt)、0=无完整条目(超时/断开)。
static int recvOne(int fd, std::vector<uint8_t>& buf, TlvMessage& msg, MediaPacket& pkt) {
    while (true) {
        uint32_t fl = 0;
        if (MediaPacketSerializer::peekFrameLength(buf.data(), buf.size(), fl)) {
            if (buf.size() >= fl) {
                MediaPacketSerializer::decode(buf.data(), fl, pkt);
                buf.erase(buf.begin(), buf.begin() + fl);
                return 2;
            }
            // 帧不完整，继续读
        } else {
            std::vector<uint8_t> work = buf;
            if (TlvProtocol::tryDecode(work, msg)) {
                buf.swap(work);
                return 1;
            }
            // TLV 不完整，继续读
        }
        char tmp[4096];
        ssize_t n = recv(fd, tmp, sizeof(tmp), 0);
        if (n <= 0) {
            return 0; // 超时或断开
        }
        buf.insert(buf.end(), tmp, tmp + n);
    }
}

// 等待指定类型的 TLV 响应；期间消费并统计媒体帧。返回是否收到。
static bool waitForResponse(int fd, std::vector<uint8_t>& buf, uint16_t expectedType,
                            int& totalFrames) {
    TlvMessage msg;
    MediaPacket pkt;
    while (true) {
        int k = recvOne(fd, buf, msg, pkt);
        if (k == 1) {
            if (msg.type == expectedType) {
                return true;
            }
            // 其他 TLV，忽略后继续等
        } else if (k == 2) {
            ++totalFrames;
        } else {
            return false;
        }
    }
}

// 收 want 个媒体帧（期间出现的 TLV 被忽略但不丢弃语义由 recvOne 保证）。
static int collectFrames(int fd, std::vector<uint8_t>& buf, int want) {
    int got = 0;
    TlvMessage msg;
    MediaPacket pkt;
    while (got < want) {
        int k = recvOne(fd, buf, msg, pkt);
        if (k == 2) {
            ++got;
        } else if (k == 0) {
            break;
        }
    }
    return got;
}

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

    // 1.socket + 2.connect（断线重连）
    int sockfd;
    struct sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr(cfg.server_ip.c_str());
    addr.sin_port = htons(cfg.server_port);
    while (true) {
        sockfd = socket(AF_INET, SOCK_STREAM, 0);
        if (sockfd < 0) {
            perror("socket");
            return 1;
        }
        if (connect(sockfd, (struct sockaddr*)&addr, sizeof(addr)) == 0) {
            std::cout << "connected!" << std::endl;
            break;
        }
        perror("connect failed, retry in 1s");
        close(sockfd);
        sleep(1);
    }

    // 2.2 设置收发超时（收/发各 2s）
    struct timeval tv;
    tv.tv_sec = 2;
    tv.tv_usec = 0;
    setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    setsockopt(sockfd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));

    std::vector<uint8_t> buf;
    int totalFrames = 0;
    uint32_t reqId = 1;

    // ---- 1) 推流 ----
    sendRequest(sockfd, static_cast<uint16_t>(MessageType::STREAM_START_REQUEST),
                std::vector<uint8_t>(), reqId++);
    if (waitForResponse(sockfd, buf,
                        static_cast<uint16_t>(MessageType::STREAM_START_RESPONSE),
                        totalFrames)) {
        std::cout << "[1] stream start resp received" << std::endl;
    } else {
        std::cout << "[1] no stream start resp (timeout)" << std::endl;
    }

    // ---- 2) 收 20 帧（流跑起来）----
    int frames = collectFrames(sockfd, buf, 20);
    totalFrames += frames;
    std::cout << "[2] received " << frames << " frames while streaming" << std::endl;

    // ---- 3) 录像开始（deviceId=7）----
    sendRequest(sockfd, static_cast<uint16_t>(MessageType::RECORD_START_REQUEST),
                makeU64BE(7), reqId++);
    if (waitForResponse(sockfd, buf,
                        static_cast<uint16_t>(MessageType::RECORD_START_RESPONSE),
                        totalFrames)) {
        std::cout << "[3] record start resp received" << std::endl;
    } else {
        std::cout << "[3] no record start resp (timeout)" << std::endl;
    }

    // ---- 4) 录像期间再收 30 帧 ----
    frames = collectFrames(sockfd, buf, 30);
    totalFrames += frames;
    std::cout << "[4] received " << frames << " frames while recording" << std::endl;

    // ---- 5) 录像停止 ----
    sendRequest(sockfd, static_cast<uint16_t>(MessageType::RECORD_STOP_REQUEST),
                std::vector<uint8_t>(), reqId++);
    if (waitForResponse(sockfd, buf,
                        static_cast<uint16_t>(MessageType::RECORD_STOP_RESPONSE),
                        totalFrames)) {
        std::cout << "[5] record stop resp received" << std::endl;
    } else {
        std::cout << "[5] no record stop resp (timeout)" << std::endl;
    }

    // ---- 6) 停流 ----
    sendRequest(sockfd, static_cast<uint16_t>(MessageType::STREAM_STOP_REQUEST),
                std::vector<uint8_t>(), reqId++);
    if (waitForResponse(sockfd, buf,
                        static_cast<uint16_t>(MessageType::STREAM_STOP_RESPONSE),
                        totalFrames)) {
        std::cout << "[6] stream stop resp received" << std::endl;
    } else {
        std::cout << "[6] no stream stop resp (timeout)" << std::endl;
    }

    std::cout << "total media frames received: " << totalFrames << std::endl;

    // 4.close
    close(sockfd);
    return 0;
}
