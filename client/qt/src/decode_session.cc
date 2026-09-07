#include "decode_session.h"

namespace smart_home {
namespace client {

DecodeSession::DecodeSession(std::unique_ptr<IDecoder> decoder)
    : _decoder(std::move(decoder)), _frames(16), _running(false), _opened(false) {}

DecodeSession::~DecodeSession() {
    stop();
}

void DecodeSession::start() {
    _running = true;
}

void DecodeSession::feed(const protocol::MediaPacket &pkt) {
    if (!_running.load()) return;

    if (!_opened) {                     // 第一个包初始化解码器
        if (!_decoder->open(pkt)) return;
        _opened = true;
    }
    Frame f;
    if (_decoder->decode(pkt, f)) {
        _frames.push(std::move(f));     // 满时自动丢最老
    }
}

bool DecodeSession::nextFrame(Frame &out) {
    return _frames.pop(out);
}

void DecodeSession::stop() {
    if (!_running.exchange(false)) return;
    _decoder->close();
    _frames.close();   // 唤醒阻塞在 nextFrame 的线程
}

}  // namespace client
}  // namespace smart_home