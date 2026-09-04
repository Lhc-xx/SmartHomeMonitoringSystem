#ifndef SMART_HOME_PROTOCOL_MEDIA_PACKET_H
#define SMART_HOME_PROTOCOL_MEDIA_PACKET_H

// ============================================================================
// media_packet.h —— 媒体包结构（角色 C 维护，服务端与 Qt 端共享的唯一格式）
//
// 这一阶段只定义"数据结构"，先不涉及"怎么变成字节"（序列化放到第三步）。
// 服务端把 FFmpeg 读到的 AVPacket 的关键字段"搬运"进来，Qt 端再"搬运"回去。
// ============================================================================

#include <cstdint>   // int64_t / uint8_t / uint32_t 等定宽整数类型
#include <vector>    // std::vector：存放压缩数据
#include <cstddef>   // size_t

namespace smart_home {
namespace protocol {

// ---------------------------------------------------------------------------
// 标志位常量：flags 字段按"位"使用，多个标志可用 | 组合。
// ---------------------------------------------------------------------------
enum : uint8_t {
    // 关键帧（I 帧）：不依赖其它帧就能独立解码的一帧。
    // 用途：录像切片要从关键帧开始切；客户端丢包后从下一个关键帧恢复画面。
    kMediaFlagKeyFrame = 0x01,
};

// ---------------------------------------------------------------------------
// MediaPacket：一个媒体数据单元
// ---------------------------------------------------------------------------
struct MediaPacket {
    int streamIndex;             // 流索引：通常 0=视频流、1=音频流（FFmpeg 约定）
    int64_t pts;                 // 展示时间戳（单位：流的 time_base，后面流建立时约定）
    int64_t dts;                 // 解码时间戳（同上）
    uint32_t duration;           // 本包时长（time_base 单位）
    uint8_t flags;               // 标志位：见上面的 kMediaFlagKeyFrame
    std::vector<uint8_t> data;   // 压缩后的媒体数据（H.264/H.265/AAC 等一小段）

    // 构造函数：给所有字段一个确定的初始值，避免"未初始化"的随机值
    MediaPacket()
        : streamIndex(0), pts(0), dts(0), duration(0), flags(0) {}

    // 便捷判断：是否关键帧（把 flags 和标志位做"按位与"）
    bool isKeyFrame() const { return (flags & kMediaFlagKeyFrame) != 0; }

    // 便捷判断：负载是否为空
    bool empty() const { return data.empty(); }

    // 清空并复位所有字段（对象复用、避免反复分配内存时调用）
    void clear() {
        streamIndex = 0;
        pts = 0;
        dts = 0;
        duration = 0;
        flags = 0;
        data.clear();
    }
};

// ---------------------------------------------------------------------------
// MediaPacketSerializer：把 MediaPacket 序列化成"带长度前缀的帧"，以及反序列化。
// 服务端用 encode() 打包发送，Qt 端用 decode() 拆包还原。
// ---------------------------------------------------------------------------
class MediaPacketSerializer {
public:
    // 用 enum（编译期常量）代替 static const，避免 C++11 链接期未定义问题
    enum : uint32_t { kMagic = 0x4D504B54 };   // 魔数，ASCII 为 "MPKT"
    enum : uint8_t  { kVersion = 1 };          // 协议版本
    enum : size_t   {
        kFrameLengthSize = 4,   // 帧长前缀占 4 字节
        kHeaderSize = 34,       // 帧长之后、负载之前的固定头字节数
    };
    enum : uint32_t { kMaxPayloadSize = 8u * 1024u * 1024u };  // 负载上限 8MB，防恶意包

    // 序列化：把一个 MediaPacket 编码成完整帧，追加到 out 末尾。成功返回 true。
    static bool encode(const MediaPacket &pkt, std::vector<uint8_t> &out);

    // 反序列化：从一段字节解码一帧。返回本帧字节数；失败（魔数错/长度非法/数据不足）返回 0。
    static size_t decode(const uint8_t *frame, size_t frameLen, MediaPacket &out);

    // 只读开头 4 字节的帧长（第 4 步重组器用它找帧边界）。不足 4 字节或长度非法返回 false。
    //返回false:不足4字节/帧长非法/（已有魔术字节）魔数不匹配
    static bool peekFrameLength(const uint8_t *data, size_t len, uint32_t &frameLen);
};

}  // namespace protocol
}  // namespace smart_home

#endif  // SMART_HOME_PROTOCOL_MEDIA_PACKET_H