#ifndef CLIENT_QT_FRAME_H
#define CLIENT_QT_FRAME_H

// ============================================================================
//  解码帧结构 + 解码器抽象接口
//
// 和 FFmpeg 解耦：这里用自己轻量的 Frame，不依赖 AVFrame。
// 真 FFmpegDecoder 里由 sws_scale 把 YUV 转成 RGBA 填进来；
// MockDecoder 直接造测试帧。上层（显示）只认 Frame，不关心谁产的。
// ============================================================================

#include <cstdint>
#include <vector>

#include "protocol/media_packet.h"

namespace smart_home {
namespace client {

// 一帧解码后的图像
struct Frame {
    int width;                 // 宽
    int height;                // 高
    int64_t pts;               // 时间戳（继承自 MediaPacket）
    std::vector<uint8_t> rgba; // RGBA 像素，大小 = width*height*4
};

// 解码器抽象接口（真 FFmpegDecoder 和 MockDecoder 都实现它）
class IDecoder {
public:
    virtual ~IDecoder() {}

    // 用第一个媒体包初始化（真实解码器在这里从流里拿到编码信息）
    virtual bool open(const protocol::MediaPacket &firstPkt) = 0;

    // 解码一个媒体包，成功输出一帧返回 true
    virtual bool decode(const protocol::MediaPacket &pkt, Frame &out) = 0;

    virtual void close() = 0;
};

}  // namespace client
}  // namespace smart_home

#endif  // CLIENT_QT_FRAME_H