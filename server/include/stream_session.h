#ifndef STREAM_SESSION_H
#define STREAM_SESSION_H

#include "connection.h"
#include "media/media_source.h"

#include <atomic>
#include <cstddef>
#include <memory>
#include <mutex>
#include <thread>

namespace smart_home {

// 流会话：绑定一个客户端连接 + 一个媒体源，持续拉包转发。
class StreamSession {
public:
    StreamSession(std::shared_ptr<Connection> conn,
                  std::unique_ptr<media::MediaSource> source);
    ~StreamSession();

    bool start();   // 启动拉流转发线程
    void stop();    // 停止（回收线程）

    // ---- 录像开关（角色 A：任务绑定）----
    // 创建录像文件并开始写入；filePath 由调用方按目录/设备生成。
    bool startRecord(const std::string &filePath);
    // 停止录像并关闭文件；输出已写字节数（用于日志/元数据）。
    bool stopRecord(std::size_t &bytesWritten);
    bool isRecording() const;

private:
    void runLoop(); // 拉流循环：readPacket -> 序列化 -> 发送（+ 录像落盘）

    std::shared_ptr<Connection> _conn;
    std::unique_ptr<media::MediaSource> _source;
    std::thread _thread;
    std::atomic<bool> _running;

    // 录像写入器：nullptr 表示未录像；换入换出由 _recordMutex 保护。
    mutable std::mutex _recordMutex;
    std::shared_ptr<class Recorder> _recorder;
};

} // namespace smart_home

#endif