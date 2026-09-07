// ffmpeg_decoder.cc —— 真 FFmpeg 解码实现（角色 C 维护）

#include "ffmpeg_decoder.h"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/imgutils.h>
#include <libavutil/frame.h>
#include <libavutil/pixfmt.h>
#include <libswscale/swscale.h>
}

#include <cstring>

namespace smart_home {
namespace client {

FFmpegDecoder::FFmpegDecoder()
    : _ctx(nullptr), _frame(nullptr), _rgbFrame(nullptr), _sws(nullptr),
      _width(0), _height(0), _pixFmt(-1) {}

FFmpegDecoder::~FFmpegDecoder() {
    close();
}

bool FFmpegDecoder::open(const protocol::MediaPacket &firstPkt) {
    (void)firstPkt;  // H.264 Annex-B 的 SPS/PPS 在带内，首个包无需额外参数

    // 找 H.264 解码器（监控摄像头通常输出 H.264）
    const AVCodec *codec = avcodec_find_decoder(AV_CODEC_ID_H264);
    if (codec == nullptr) {
        return false;
    }

    _ctx = avcodec_alloc_context3(codec);
    if (_ctx == nullptr) {
        return false;
    }

    // Annex-B 裸流：SPS/PPS 随关键帧在带内携带，无需 extradata
    if (avcodec_open2(_ctx, codec, nullptr) < 0) {
        close();
        return false;
    }

    _frame = av_frame_alloc();
    _rgbFrame = av_frame_alloc();
    if (_frame == nullptr || _rgbFrame == nullptr) {
        close();
        return false;
    }
    return true;
}

bool FFmpegDecoder::decode(const protocol::MediaPacket &pkt, Frame &out) {
    if (_ctx == nullptr || _frame == nullptr) {
        return false;
    }

    // 把 MediaPacket 的压缩字节装进 AVPacket
    AVPacket avpkt;
    av_init_packet(&avpkt);
    avpkt.data = const_cast<uint8_t *>(pkt.data.data());
    avpkt.size = static_cast<int>(pkt.data.size());
    avpkt.pts = pkt.pts;
    avpkt.dts = pkt.dts;
    if (pkt.isKeyFrame()) {
        avpkt.flags |= AV_PKT_FLAG_KEY;
    }

    // 送入一个包；EAGAIN 表示内部还有帧没取完，先继续 receive 也无妨
    int ret = avcodec_send_packet(_ctx, &avpkt);
    if (ret < 0 && ret != AVERROR(EAGAIN)) {
        return false;
    }

    // 一个包可能解出 0/1/多 帧，循环取帧，输出最后一帧
    bool got = false;
    while (true) {
        ret = avcodec_receive_frame(_ctx, _frame);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
            break;
        }
        if (ret < 0) {
            break;
        }

        if (!ensureScaler(_frame->width, _frame->height, _frame->format)) {
            break;
        }

        // YUV -> RGBA
        sws_scale(_sws, _frame->data, _frame->linesize, 0, _frame->height,
                  _rgbFrame->data, _rgbFrame->linesize);

        out.width = _width;
        out.height = _height;
        out.pts = (_frame->pts != AV_NOPTS_VALUE) ? _frame->pts : pkt.pts;
        out.rgba.resize(static_cast<size_t>(_width) * _height * 4);

        // RGBA 每行可能按字节对齐，逐行拷贝去掉可能的 padding
        const uint8_t *src = _rgbFrame->data[0];
        uint8_t *dst = out.rgba.data();
        const int rowBytes = _width * 4;
        for (int y = 0; y < _height; ++y) {
            std::memcpy(dst + static_cast<size_t>(y) * rowBytes,
                        src + static_cast<size_t>(y) * _rgbFrame->linesize[0],
                        rowBytes);
        }
        got = true;
    }
    return got;
}

bool FFmpegDecoder::ensureScaler(int width, int height, int pixFmt) {
    if (width <= 0 || height <= 0) {
        return false;
    }
    if (_sws != nullptr && width == _width && height == _height && pixFmt == _pixFmt) {
        return true;
    }

    // 释放旧转换上下文
    if (_sws != nullptr) {
        sws_freeContext(_sws);
        _sws = nullptr;
    }
    if (_rgbFrame != nullptr) {
        av_freep(&_rgbFrame->data[0]);
        av_frame_unref(_rgbFrame);
    }

    _width = width;
    _height = height;
    _pixFmt = pixFmt;

    _sws = sws_getContext(width, height, static_cast<AVPixelFormat>(pixFmt),
                          width, height, AV_PIX_FMT_RGBA,
                          SWS_BILINEAR, nullptr, nullptr, nullptr);
    if (_sws == nullptr) {
        return false;
    }

    const int ret = av_image_alloc(_rgbFrame->data, _rgbFrame->linesize,
                                   width, height, AV_PIX_FMT_RGBA, 1);
    if (ret < 0) {
        return false;
    }
    return true;
}

void FFmpegDecoder::close() {
    if (_sws != nullptr) {
        sws_freeContext(_sws);
        _sws = nullptr;
    }
    if (_rgbFrame != nullptr) {
        av_freep(&_rgbFrame->data[0]);
        av_frame_free(&_rgbFrame);
    }
    if (_frame != nullptr) {
        av_frame_free(&_frame);
    }
    if (_ctx != nullptr) {
        avcodec_free_context(&_ctx);
    }
    _width = 0;
    _height = 0;
    _pixFmt = -1;
}

}  // namespace client
}  // namespace smart_home
