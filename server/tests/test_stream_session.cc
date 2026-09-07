// test_stream_session.cc —— 第九步：流会话（用 Mock 源测整条转发链路）

#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include "media/mock_media_source.h"
#include "media/stream_session.h"
#include "protocol/media_packet.h"

using smart_home::media::MediaSource;
using smart_home::media::MockMediaSource;
using smart_home::media::StreamSession;
using smart_home::protocol::MediaPacket;
using smart_home::protocol::MediaPacketSerializer;

static int g_failures = 0;
static void expect(bool cond, const char *msg) {
    if (cond) {
        std::cout << "  [ok]   " << msg << std::endl;
    } else {
        std::cout << "  [FAIL] " << msg << std::endl;
        ++g_failures;
    }
}

int main() {
    std::cout << "=== StreamSession test begin ===" << std::endl;

    // 注入 Mock 源（面向接口，不碰 FFmpeg）
    StreamSession session(std::unique_ptr<MediaSource>(new MockMediaSource()));

    // 1) 启动
    expect(session.start("mock://camera0"), "start returns true");
    expect(session.isRunning(), "isRunning == true after start");
    expect(!session.start("mock://again"), "start again while running returns false");

    // 2) 连续取 10 段字节，验证都能解回合法媒体包且 pts 递增
    {
        int got = 0;
        bool valid = true, ptsOk = true;
        int64_t lastPts = -1;
        while (got < 10) {
            std::vector<uint8_t> bytes;
            if (!session.nextSendPacket(bytes)) break;
            MediaPacket p;
            size_t used = MediaPacketSerializer::decode(bytes.data(), bytes.size(), p);
            if (used != bytes.size()) { valid = false; break; }
            if (p.pts <= lastPts) ptsOk = false;
            lastPts = p.pts;
            ++got;
        }
        expect(got == 10, "receive 10 serialized frames");
        expect(valid, "each frame decodes back to a valid MediaPacket");
        expect(ptsOk, "pts strictly increasing");
    }

    // 3) 断流重连：reconnect 后 pts 从 0 重新开始（Mock 的 reconnect 会重置帧号）
    {
        expect(session.reconnect(), "reconnect returns true");
        std::vector<uint8_t> bytes;
        session.nextSendPacket(bytes);
        MediaPacket p;
        MediaPacketSerializer::decode(bytes.data(), bytes.size(), p);
        expect(p.pts == 0, "after reconnect, pts restarts from 0");
    }

    // 4) 停止后取不到数据
    {
        session.stop();
        expect(!session.isRunning(), "isRunning == false after stop");
        std::vector<uint8_t> bytes;
        expect(!session.nextSendPacket(bytes), "nextSendPacket returns false after stop");
    }

    std::cout << "=== StreamSession test end ===" << std::endl;
    if (g_failures == 0) {
        std::cout << "stream_session test passed." << std::endl;
        return 0;
    }
    std::cout << "stream_session test FAILED: " << g_failures << " items." << std::endl;
    return 1;
}