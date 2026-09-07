//  假解码器实现

#include "mock_decoder.h"

namespace smart_home {
namespace client {

MockDecoder::MockDecoder() : _width(0), _height(0), _opened(false) {}

bool MockDecoder::open(const protocol::MediaPacket &firstPkt) {
   (void)firstPkt;   // 显式标记"这个参数暂不使用"
    _width = 64;
    _height = 48;
    _opened = true;
    return true;
}

bool MockDecoder::decode(const protocol::MediaPacket &pkt, Frame &out) {
    if (!_opened) {
        return false;
    }

    out.width = _width;
    out.height = _height;
    out.pts = pkt.pts;               // 时间戳直接继承
    out.rgba.resize(_width * _height * 4);

    // 造一帧可预测的图：每帧颜色随 pts 变化，便于测试验证
    uint8_t r = static_cast<uint8_t>(pkt.pts & 0xFF);
    for (size_t i = 0; i < out.rgba.size(); i += 4) {
        out.rgba[i + 0] = r;          // R
        out.rgba[i + 1] = 0x80;       // G
        out.rgba[i + 2] = 0x40;       // B
        out.rgba[i + 3] = 0xFF;       // A（不透明）
    }
    return true;
}

void MockDecoder::close() {
    _opened = false;
}

}  // namespace client
}  // namespace smart_home