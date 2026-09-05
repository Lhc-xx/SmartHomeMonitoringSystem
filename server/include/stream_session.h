#ifndef STREAM_SESSION_H
#define STREAM_SESSION_H

#include "connection.h"
#include "media/media_source.h"

#include <atomic>
#include <memory>
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

private:
    void runLoop(); // 拉流循环：readPacket -> 序列化 -> 发送

    std::shared_ptr<Connection> _conn;
    std::unique_ptr<media::MediaSource> _source;
    std::thread _thread;
    std::atomic<bool> _running;
};

} // namespace smart_home

#endif