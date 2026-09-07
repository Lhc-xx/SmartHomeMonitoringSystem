//媒体包序列化实现（角色C维护）

#include "protocol/media_packet.h"

#include <cstring> //std::memcpy

#include <cstring> //std::memcpy

namespace smart_home {
    namespace protocol {

        //-------------------------
        //大端序读写辅助函数（匿名命名空间 = 只在本文件可见，相当于static）
        //
        //为什么手动逐字节写，而不是直接 memcpy int;
        // 1)x86是小端序，网络要的是大端序，必须转换；
        // 2）结构体成员间可能有"填充字节",直接memcpy会把填充也发出去
        //-------------------------

        //写4字节大端序
        namespace {
        inline void writeU32BE(uint8_t *p,uint32_t v) {
            p[0] = static_cast<uint8_t>((v >> 24) & 0xFF); //最高字节序放在最前面
            p[1] = static_cast<uint8_t>((v >> 16) & 0xFF);
            p[2] = static_cast<uint8_t>((v >> 8) & 0xFF);
            p[3] = static_cast<uint8_t>(v & 0xFF); //最低字节放最后
        }

        //写8字节大端序
        inline void writeU64BE(uint8_t *p,uint64_t v) {
            for (int i = 0;i < 8; ++i) {
                p[i] = static_cast<uint8_t>((v >> ((7 - i) * 8)) & 0xFF);
            }
        }

        //读4字节大端序
        inline uint32_t readU32BE(const uint8_t *p) {
            return (static_cast<uint32_t>(p[0]) << 24) |
            (static_cast<uint32_t>(p[1]) << 16) |
            (static_cast<uint32_t>(p[2]) << 8) |
            (static_cast<uint32_t>(p[3])); 
        }

        //读8字节大端序
        inline uint64_t readU64BE(const uint8_t *p) {
            uint64_t v = 0;
            for (int i = 0; i < 8; ++i) {
                v = (v << 8) | p[i]; //每读一字节，结果左移8位在拼上新字节
            }
            return v;
        }

// 帧内各字段的字节偏移（和头文件里的帧格式表一一对应）
const size_t kOffMagic      = 4;
const size_t kOffVersion    = 8;
const size_t kOffFlags      = 9;
const size_t kOffStreamIdx  = 10;
const size_t kOffPts        = 14;
const size_t kOffDts        = 22;
const size_t kOffDuration   = 30;
const size_t kOffPayloadLen = 34;
const size_t kOffPayload    = 38;   // = 4 帧长 + 34 固定头

}  // namespace

// ---------------------------------------------------------------------------
// encode：序列化
// ---------------------------------------------------------------------------
bool MediaPacketSerializer::encode(const MediaPacket &pkt, std::vector<uint8_t> &out) {
    // 负载超长直接拒绝，防止异常数据把内存打爆
    if (pkt.data.size() > kMaxPayloadSize) {
        return false;
    }

    // 整帧长度 = 4(帧长) + 34(固定头) + 负载字节数
    uint32_t frameLen = static_cast<uint32_t>(kFrameLengthSize + kHeaderSize + pkt.data.size());

    // 在 out 末尾预留 frameLen 字节，拿到写入起点（"追加"而不是覆盖，便于连续塞多个包）
    size_t start = out.size();
    out.resize(start + frameLen);
    uint8_t *p = out.data() + start;

    // 逐字段写入（大端序）
    writeU32BE(p + 0, frameLen);                                    // [0..3]   帧长
    writeU32BE(p + kOffMagic, kMagic);                              // [4..7]   魔数
    p[kOffVersion] = kVersion;                                      // [8]      版本
    p[kOffFlags] = pkt.flags;                                       // [9]      标志位
    writeU32BE(p + kOffStreamIdx,
               static_cast<uint32_t>(pkt.streamIndex));             // [10..13] 流索引
    writeU64BE(p + kOffPts, static_cast<uint64_t>(pkt.pts));        // [14..21] pts
    writeU64BE(p + kOffDts, static_cast<uint64_t>(pkt.dts));        // [22..29] dts
    writeU32BE(p + kOffDuration, pkt.duration);                     // [30..33] 时长
    writeU32BE(p + kOffPayloadLen,
               static_cast<uint32_t>(pkt.data.size()));             // [34..37] 负载长度

    // 负载原始字节，直接拷贝
    if (!pkt.data.empty()) {
        std::memcpy(p + kOffPayload, pkt.data.data(), pkt.data.size());
    }
    return true;
}

// ---------------------------------------------------------------------------
// decode：反序列化
// ---------------------------------------------------------------------------
size_t MediaPacketSerializer::decode(const uint8_t *frame, size_t frameLen, MediaPacket &out) {
    // 连 4 字节帧长都没有，肯定不是完整帧
    if (frameLen < kFrameLengthSize) {
        return 0;
    }

    // 读出帧长并做合法性检查
    uint32_t total = readU32BE(frame);
    if (total < kFrameLengthSize + kHeaderSize) {
        return 0;   // 帧长比最小帧还小，数据损坏
    }
    if (total > frameLen) {
        return 0;   // 帧还没收全（调用方应攒够字节再调）
    }

    // 魔数校验：错位/损坏的字节几乎不可能恰好是 "MPKT"
    if (readU32BE(frame + kOffMagic) != kMagic) {
        return 0;
    }
    // 版本校验：未来协议升级后可据此判断兼容性
    if (frame[kOffVersion] != kVersion) {
        return 0;
    }

    // 逐字段读出
    out.flags       = frame[kOffFlags];
    out.streamIndex = static_cast<int32_t>(readU32BE(frame + kOffStreamIdx));
    out.pts         = static_cast<int64_t>(readU64BE(frame + kOffPts));
    out.dts         = static_cast<int64_t>(readU64BE(frame + kOffDts));
    out.duration    = readU32BE(frame + kOffDuration);
    uint32_t payloadLen = readU32BE(frame + kOffPayloadLen);

    // 负载长度不能超出本帧实际范围（防越界读）
    if (payloadLen > total - (kFrameLengthSize + kHeaderSize)) {
        return 0;
    }

    // 拷贝负载
    out.data.assign(frame + kOffPayload, frame + kOffPayload + payloadLen);

    return total;   // 返回本帧字节数，供上层移动指针
}

// ---------------------------------------------------------------------------
// peekFrameLength：只读帧长前缀（第 4 步重组器用）
// ---------------------------------------------------------------------------
bool MediaPacketSerializer::peekFrameLength(const uint8_t *data, size_t len, uint32_t &frameLen) {
    if (len < kFrameLengthSize) {
        return false;   // 连帧长前缀都不够 4 字节
    }
    frameLen = readU32BE(data);

    // 帧长必须在 [最小帧, 最小帧+最大负载] 之间，否则视为损坏/伪造
    if (frameLen < kFrameLengthSize + kHeaderSize) {
        return false;
    }
    if (frameLen > kFrameLengthSize + kHeaderSize + kMaxPayloadSize) {
        return false;
    }

    // 若已收到魔数字节（帧长 4 + 魔数 4 = 8 字节），顺带校验魔数，
    // 避免"乱码里碰巧出现一个合法长度"被误判成一帧开头、导致重组器卡住。
    if (len >= kFrameLengthSize + 4) {
        if (readU32BE(data + kOffMagic) != kMagic) {
            return false;
        }
    }
    return true;
}

}  // namespace protocol
}  // namespace smart_home