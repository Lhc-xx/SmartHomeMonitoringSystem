// stream_session.cc —— 流会话实现（角色 C 维护，实时转发核心，统一版）

#include "media/stream_session.h"

#include <chrono>
#include <mutex>

#include "protocol/media_packet.h"   // MediaPacketSerializer
#include "recorder.h"                // Recorder（录像落盘）

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
    if (_running.exchange(false)) {
        _source->close();      // 让阻塞在 readPacket 的线程返回
        _sendQueue.close();    // 让阻塞在 push 的线程返回，也让 nextSendPacket 返回 false
        if (_pullThread.joinable()) {
            _pullThread.join();
        }
    }
    // 兜底：停止时若仍在录像，关闭文件避免句柄泄漏
    std::size_t ignored = 0;
    stopRecord(ignored);
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

bool StreamSession::startRecord(const std::string &filePath) {
    std::lock_guard<std::mutex> guard(_recordMutex);
    if (_recorder) {
        return false;   // 已在录像，不重复开启
    }
    std::shared_ptr<Recorder> rec(new Recorder());
    if (!rec->open(filePath)) {
        return false;   // 文件创建失败
    }
    _recorder = rec;
    return true;
}

bool StreamSession::stopRecord(std::size_t &bytesWritten) {
    std::lock_guard<std::mutex> guard(_recordMutex);
    if (!_recorder) {
        return false;   // 未在录像
    }
    bytesWritten = _recorder->bytesWritten();
    _recorder->close();
    _recorder.reset();
    return true;
}

bool StreamSession::isRecording() const {
    std::lock_guard<std::mutex> guard(_recordMutex);
    return _recorder != nullptr;
}

// 拉流线程主循环：生产者
void StreamSession::pullLoop() {
    while (_running.load()) {
        protocol::MediaPacket pkt;
        if (!_source->readPacket(pkt)) {
            // 没数据或断流：尝试重连，失败则结束会话
            if (!_source->reconnect()) {
                break;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            continue;
        }

        std::vector<uint8_t> bytes;
        if (!protocol::MediaPacketSerializer::encode(pkt, bytes)) {
            continue;
        }

        // 录像落盘：与发送同一帧
        {
            std::lock_guard<std::mutex> guard(_recordMutex);
            if (_recorder) {
                _recorder->write(bytes);
            }
        }

        // 分发：设了 sink 就直接推送（Reactor 直发连接），否则入队等消费者取
        if (_sink) {
            _sink(bytes);
            // 直推模式无缓冲背压，限速避免 Mock 源刷爆连接（≈33fps）
            std::this_thread::sleep_for(std::chrono::milliseconds(30));
        } else if (!_sendQueue.push(bytes)) {
            break;          // 队列已关闭
        }
    }
}

}  // namespace media
}  // namespace smart_home
