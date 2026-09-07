#ifndef SERVER_MEDIA_MOCK_MEDIA_SOURCE_H
#define SERVER_MEDIA_MOCK_MEDIA_SOURCE_H

// ============================================================================
// mock_media_source.h —— 假媒体源（角色 C 维护，测试用）
//
// 不依赖 FFmpeg、不依赖真实摄像头，按固定规则"凭空造出"一串媒体包，
// 用来在开发阶段测试整条拉流→转发→重组链路。
// ============================================================================

#include <cstdint>   // int64_t
#include <string>

#include "media/media_source.h"

namespace smart_home {
namespace media {

class MockMediaSource : public MediaSource {
public:
    // keyFrameInterval：每隔多少帧生成一个关键帧（默认 30）
    explicit MockMediaSource(int keyFrameInterval = 30);

    bool open(const std::string &url) override;
    bool readPacket(protocol::MediaPacket &out) override;
    void close() override;
    bool reconnect() override;

    // 测试辅助：当前已生成多少帧
    int64_t frameCount() const { return _frameIndex; }

private:
    std::string _url;            // 记住打开的 url，供 reconnect 用
    int64_t _frameIndex;         // 已生成帧数（同时当作 pts 用）
    bool _opened;                // 是否已打开
    int _keyFrameInterval;       // 关键帧间隔
};

}  // namespace media
}  // namespace smart_home

#endif  // SERVER_MEDIA_MOCK_MEDIA_SOURCE_H