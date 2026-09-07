#ifndef SERVER_MEDIA_STREAM_SESSION_H
#define SERVER_MEDIA_STREAM_SESSION_H

// ============================================================================
// stream_session.h —— 流会话（角色 C 维护，实时转发核心）
//
// 把「拉流 → 序列化 → 缓冲 → 交给网络发送」串起来。
// 内部一个拉流线程（生产者）+ 一个 RingBuffer；A 通过 nextSendPacket 当消费者。
//
// 与 A 的接口：A 拿到本对象后，只需 start / nextSendPacket / stop / reconnect，
// 完全不需要知道 FFmpeg、RingBuffer、MediaPacket 的细节。
// ============================================================================

#include <atomic>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include "common/ring_buffer.h"
#include "media/media_source.h"

namespace smart_home {
namespace media {

class StreamSession {
public:
    // 注入媒体源（面向接口，不关心具体是 Mock 还是 FFmpeg）
    explicit StreamSession(std::unique_ptr<MediaSource> source);

    // 析构自动停止，保证线程和资源释放
    ~StreamSession();

    // 启动：打开源 + 启动拉流线程。成功返回 true。
    bool start(const std::string &url);

    // 停止：停线程、关源、关缓冲（之后本对象不可复用）
    void stop();

    // 【给 A 用】取出一段已序列化的字节（要发给客户端的）。阻塞直到有数据或已停止。
    bool nextSendPacket(std::vector<uint8_t> &out);

    // 断流重连：运行中让底层源重新建立连接（对应分工计划"断流重连"）
    bool reconnect();

    bool isRunning() const { return _running.load(); }

private:
    void pullLoop();   // 拉流线程主循环

    std::unique_ptr<MediaSource> _source;          // 媒体源（抽象接口）
    common::RingBuffer<std::vector<uint8_t>> _sendQueue;  // 序列化字节缓冲
    std::thread _pullThread;                       // 拉流线程
    std::string _url;                              // 记住 url，供 reconnect
    std::atomic<bool> _running;                    // 运行标志
};

}  // namespace media
}  // namespace smart_home

#endif  // SERVER_MEDIA_STREAM_SESSION_H