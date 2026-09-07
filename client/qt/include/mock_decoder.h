#ifndef CLIENT_QT_MOCK_DECODER_H
#define CLIENT_QT_MOCK_DECODER_H

// ============================================================================
// mock_decoder.h —— 假解码器（角色 C 维护，测试骨架用）
//
// 不依赖 FFmpeg，根据 MediaPacket 造出一帧纯色/可预测的 RGBA 图像，
// 用来在没有真解码器时验证"解码线程 → 帧队列 → 显示线程"整条链路。
// ============================================================================

#include "frame.h"

namespace smart_home {
namespace client {

class MockDecoder : public IDecoder {
public:
    MockDecoder();   // 构造函数

    bool open(const protocol::MediaPacket &firstPkt) override;
    bool decode(const protocol::MediaPacket &pkt, Frame &out) override;
    void close() override;

private:
    int _width;     // 输出帧宽
    int _height;    // 输出帧高
    bool _opened;
};

}  // namespace client
}  // namespace smart_home

#endif  // CLIENT_QT_MOCK_DECODER_H