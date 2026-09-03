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

}  // namespace protocol
}  // namespace smart_home

#endif  // SMART_HOME_PROTOCOL_MEDIA_PACKET_H