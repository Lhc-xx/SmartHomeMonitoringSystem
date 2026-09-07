// stream_session.cc —— 流会话实现（角色 C 维护）

#include "media/stream_session.h"

#include <chrono>

#include "protocol/media_packet.h"   // MediaPacketSerializer

namespace smart_home {
namespace media {

StreamSession::StreamSession(std::unique_ptr<MediaSource> source)
    : _source(std::move(source)),
      _sendQueue(64),      // 缓冲 64 段序列化帧
      _running(false) {}

StreamSession::~StreamSession() {
    stop();                // 对象销毁时确保线程和资源都释放
}

bool StreamSession::start(const std::string &url) {
    if (_running.load()) {
        return false;      // 已在运行
    }
    _url = url;
    if (!_source->open(url)) {
        return false;      // 打开源失败
    }
    _running = true;
    _pullThread = std::thread(&StreamSession::pullLoop, this);
    return true;
}

void StreamSession::stop() {
    if (!_running.exchange(false)) {
        return;            // 已经停了，幂等
    }
    _source->close();      // 让阻塞在 readPacket 的线程返回
    _sendQueue.close();    // 让阻塞在 push 的线程返回，也让 nextSendPacket 返回 false
    if (_pullThread.joinable()) {
        _pullThread.join();
    }
}

bool StreamSession::nextSendPacket(std::vector<uint8_t> &out) {
    return _sendQueue.pop(out);   // 阻塞取，队列 close 后返回 false
}

bool StreamSession::reconnect() {
    if (!_running.load()) {
        return false;
    }
    return _source->reconnect();  // 运行中断流重连：只重建底层源连接
}

// 拉流线程主循环：生产者
void StreamSession::pullLoop() {
    while (_running.load()) {
        protocol::MediaPacket pkt;
        if (!_source->readPacket(pkt)) {
            // 暂时没数据（或读到头了）：睡一小会儿再试，避免空转占满 CPU
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            continue;
        }

        std::vector<uint8_t> bytes;
        if (protocol::MediaPacketSerializer::encode(pkt, bytes)) {
            // push 阻塞直到有空间；返回 false 说明队列被 close，退出
            if (!_sendQueue.push(bytes)) {
                break;
            }
        }
    }
}

}  // namespace media
}  // namespace smart_home