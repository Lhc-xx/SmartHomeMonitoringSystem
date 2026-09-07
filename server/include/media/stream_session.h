#ifndef SERVER_MEDIA_STREAM_SESSION_H
#define SERVER_MEDIA_STREAM_SESSION_H

// ============================================================================
// stream_session.h —— 流会话（角色 C 维护，实时转发核心，统一版）
//
// 把「拉流 → 序列化 → 缓冲 → 交给网络发送」串起来。
// 内部一个拉流线程（生产者）+ 一个 RingBuffer。
//
// 两种消费方式（二选一）：
//   1) 解耦模式（默认）：A 通过 nextSendPacket 当消费者，自己决定何时发送；
//   2) 直推模式：A 先 setSink(...) 注入回调，拉流线程直接把帧交给回调
//      （Reactor 用它直接 conn->sendData，省一个转发线程）。
//
// 与 A 的接口：A 拿到本对象后，只需 start / (nextSendPacket 或 setSink) /
//   stop / reconnect / startRecord / stopRecord，不需要知道底层细节。
// ============================================================================

#include <atomic>
#include <cstddef>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "common/ring_buffer.h"
#include "media/media_source.h"

namespace smart_home {

class Recorder;  // 录像文件写入器（定义在 recorder.h）

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

    // 【解耦模式】取出一段已序列化的字节（要发给客户端的）。阻塞直到有数据或已停止。
    bool nextSendPacket(std::vector<uint8_t> &out);

    // 【直推模式】注入发送回调；设置后拉流线程直接把每帧字节交给它。
    void setSink(std::function<void(const std::vector<uint8_t> &)> sink) { _sink = sink; }

    // 断流重连：运行中让底层源重新建立连接（对应分工计划"断流重连"）
    bool reconnect();

    bool isRunning() const { return _running.load(); }

    // ---- 录像（角色 A：任务绑定）----
    bool startRecord(const std::string &filePath);   // 创建录像文件并开始写入
    bool stopRecord(std::size_t &bytesWritten);      // 停止录像，输出已写字节数
    bool isRecording() const;

private:
    void pullLoop();   // 拉流线程主循环

    std::unique_ptr<MediaSource> _source;                    // 媒体源（抽象接口）
    common::RingBuffer<std::vector<uint8_t>> _sendQueue;     // 序列化字节缓冲（解耦模式）
    std::thread _pullThread;                                 // 拉流线程
    std::string _url;                                        // 记住 url，供 reconnect
    std::atomic<bool> _running;                              // 运行标志

    std::function<void(const std::vector<uint8_t> &)> _sink; // 可选：直推回调

    mutable std::mutex _recordMutex;                         // 保护 _recorder 换入换出
    std::shared_ptr<Recorder> _recorder;                     // 录像写入器（nullptr 表示未录像）
};

}  // namespace media
}  // namespace smart_home

#endif  // SERVER_MEDIA_STREAM_SESSION_H
