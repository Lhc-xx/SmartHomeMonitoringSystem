// ffmpeg_media_source.cc —— FFmpeg 拉流实现（角色 C 维护）

#include "media/ffmpeg_media_source.h"

// FFmpeg 头文件自带 extern "C" 保护，C++ 里可以直接包含
extern "C" {
#include <libavformat/avformat.h>   // avformat_open_input / av_read_frame / AVFormatContext
#include <libavcodec/avcodec.h>     // AVPacket / AV_PKT_FLAG_KEY
#include <libavutil/avutil.h>       // AVMEDIA_TYPE_VIDEO / av_find_best_stream
}
namespace smart_home {
namespace media {

using protocol::kMediaFlagKeyFrame;

FFmpegMediaSource::FFmpegMediaSource()
    : _fmtCtx(nullptr), _videoStreamIndex(-1), _opened(false) {}

FFmpegMediaSource::~FFmpegMediaSource() {
    close();   // 对象销毁时确保释放 FFmpeg 资源（对应"无明显泄漏"）
}

bool FFmpegMediaSource::open(const std::string &url) {
    // 拉网络流（rtsp/rtmp）前，先初始化 FFmpeg 的网络模块
    avformat_network_init();

    _url = url;

    // 第 1 步：打开输入（封装层"大管家"），根据 url 自动识别协议和格式
    int ret = avformat_open_input(&_fmtCtx, url.c_str(), nullptr, nullptr);
    if (ret < 0) {
        _fmtCtx = nullptr;
        return false;
    }

    // 第 2 步：探测流信息（填充 streams[]、时长、码率等）
    ret = avformat_find_stream_info(_fmtCtx, nullptr);
    if (ret < 0) {
        avformat_close_input(&_fmtCtx);
        _fmtCtx = nullptr;
        return false;
    }

    // 第 3 步：找"最好的视频流"的下标
    _videoStreamIndex = av_find_best_stream(_fmtCtx, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
    if (_videoStreamIndex < 0) {
        avformat_close_input(&_fmtCtx);
        _fmtCtx = nullptr;
        return false;
    }

    _opened = true;
    return true;
}

bool FFmpegMediaSource::readPacket(protocol::MediaPacket &out) {
    if (!_opened || !_fmtCtx) {
        return false;
    }

    AVPacket *pkt = av_packet_alloc();   // 分配一个 AVPacket
    if (!pkt) {
        return false;
    }

    // 循环读，跳过非视频流（如音频），直到读到视频包或出错/读完
    while (true) {
        int ret = av_read_frame(_fmtCtx, pkt);
        if (ret < 0) {
            av_packet_free(&pkt);   // 读完(EOF) 或 出错
            return false;
        }
        if (pkt->stream_index != _videoStreamIndex) {
            av_packet_unref(pkt);   // 不是视频流，丢弃继续读
            continue;
        }
        break;                      // 读到了视频包
    }

    // 把 AVPacket 的关键字段"搬运"到我们自己的 MediaPacket
    out.clear();
    out.streamIndex = 0;                                  // 只转发一路视频，统一记作 0
    out.pts = pkt->pts;                                   // 单位=流的 time_base（如 1/90000）
    out.dts = pkt->dts;
    out.duration = static_cast<uint32_t>(pkt->duration);
    out.flags = (pkt->flags & AV_PKT_FLAG_KEY) ? kMediaFlagKeyFrame : 0;
    out.data.assign(pkt->data, pkt->data + pkt->size);    // 压缩后的 H.264 字节

    av_packet_free(&pkt);
    return true;
}

void FFmpegMediaSource::close() {
    if (_fmtCtx) {
        avformat_close_input(&_fmtCtx);
        _fmtCtx = nullptr;
    }
    _videoStreamIndex = -1;
    _opened = false;
}

bool FFmpegMediaSource::reconnect() {
    close();            // 先关掉旧的
    return open(_url);  // 用记住的 url 重开
}

}  // namespace media
}  // namespace smart_home