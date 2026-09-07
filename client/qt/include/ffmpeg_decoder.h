#ifndef CLIENT_QT_FFMPEG_DECODER_H
#define CLIENT_QT_FFMPEG_DECODER_H

// ============================================================================
// ffmpeg_decoder.h —— 真 FFmpeg 解码器（角色 C 维护）
//
// 实现 IDecoder：把 MediaPacket 里的压缩视频包（H.264 Annex-B）解码成
// 一帧 RGBA 图像（Frame）。用 avcodec 解码 + sws_scale 做 YUV -> RGBA 转换。
//
// 与 MockDecoder 的区别：本类真实调用 FFmpeg，需要链接 libavcodec/libavutil/
// libswscale；因此只在 WITH_FFMPEG 打开时参与编译，其它时候用 MockDecoder。
// ============================================================================

#include "frame.h"

struct AVCodecContext;
struct AVFrame;
struct SwsContext;

namespace smart_home {
namespace client {

class FFmpegDecoder : public IDecoder {
public:
    FFmpegDecoder();
    ~FFmpegDecoder() override;

    bool open(const protocol::MediaPacket &firstPkt) override;
    bool decode(const protocol::MediaPacket &pkt, Frame &out) override;
    void close() override;

private:
    // 首次解码或分辨率/像素格式变化时（重新）建立 sws 转换上下文与 RGBA 缓冲。
    bool ensureScaler(int width, int height, int pixFmt);

    AVCodecContext *_ctx;      // 解码器上下文
    AVFrame *_frame;           // 解码后的 YUV 帧
    AVFrame *_rgbFrame;        // sws 转换出的 RGBA 帧
    SwsContext *_sws;          // 像素格式转换上下文
    int _width;                // 当前输出宽
    int _height;               // 当前输出高
    int _pixFmt;               // 当前输入像素格式
};

}  // namespace client
}  // namespace smart_home

#endif  // CLIENT_QT_FFMPEG_DECODER_H
