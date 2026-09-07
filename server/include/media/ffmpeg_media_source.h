#ifndef SERVER_MEDIA_FFMPEG_MEDIA_SOURCE_H
#define SERVER_MEDIA_FFMPEG_MEDIA_SOURCE_H

// ============================================================================
// ffmpeg_media_source.h —— FFmpeg 真拉流（角色 C 维护）
//
// 用 FFmpeg 的 avformat 从 rtsp/rtmp/文件 里拉流，实现 MediaSource 接口。
// 注意：用前置声明隐藏 FFmpeg 类型，本头文件不引入任何 FFmpeg 头。
// ============================================================================

#include <mutex>
#include <string>

#include "media/media_source.h"

struct AVFormatContext;   // 前置声明，实际定义在 libavformat 里

namespace smart_home {
namespace media {

class FFmpegMediaSource : public MediaSource {
public:
    FFmpegMediaSource();
    ~FFmpegMediaSource() override;   // 析构里 close()，保证资源释放

    bool open(const std::string &url) override;
    bool readPacket(protocol::MediaPacket &out) override;
    void close() override;
    bool reconnect() override;

private:
    std::string _url;              // 记住 url，reconnect 用
    AVFormatContext *_fmtCtx;      // FFmpeg 封装层上下文（"大管家"）
    int _videoStreamIndex;         // 视频流在容器里的下标
    bool _opened;

    // 串行化 open/readPacket/close：StreamSession 的 stop() 会从主线程调用
    // close() 释放 _fmtCtx，而拉流线程可能正阻塞在 av_read_frame 上，直接释放
    // 会导致 use-after-free（集成测试偶发段错误）。加锁后 close 等待当前读取结束。
    mutable std::mutex _mutex;
};

}  // namespace media
}  // namespace smart_home

#endif  // SERVER_MEDIA_FFMPEG_MEDIA_SOURCE_H