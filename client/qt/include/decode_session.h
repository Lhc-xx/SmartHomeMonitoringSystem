#ifndef CLIENT_QT_DECODE_SESSION_H
#define CLIENT_QT_DECODE_SESSION_H

#include <atomic>
#include <memory>

#include "common/frame_queue.h"
#include "frame.h"
#include "protocol/media_packet.h"

namespace smart_home {
namespace client {

// 解码会话：串起「解码 → 帧队列 → 显示」。
// 骨架版不自己开线程，由调用方在两个线程里分别调 feed（喂包）和 nextFrame（取帧）。
class DecodeSession {
public:
    explicit DecodeSession(std::unique_ptr<IDecoder> decoder);
    ~DecodeSession();

    void start();                              // 标记开始
    void feed(const protocol::MediaPacket &pkt);   // 喂包 + 解码 + 入队
    bool nextFrame(Frame &out);                 // 取一帧（阻塞，直到有帧或停止）
    void stop();
    bool isRunning() const { return _running.load(); }

private:
    std::unique_ptr<IDecoder> _decoder;
    common::FrameQueue<Frame> _frames;
    std::atomic<bool> _running;
    bool _opened;
};

}  // namespace client
}  // namespace smart_home

#endif