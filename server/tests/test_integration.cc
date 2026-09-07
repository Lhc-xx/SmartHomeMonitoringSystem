// 端到端集成测试（源→序列化→网络→重组→校验）

#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include "common/media_reassembler.h"
#include "media/ffmpeg_media_source.h"
#include "media/mock_media_source.h"
#include "media/stream_session.h"
#include "protocol/media_packet.h"

using smart_home::common::MediaReassembler;
using smart_home::media::FFmpegMediaSource;
using smart_home::media::MediaSource;
using smart_home::media::MockMediaSource;
using smart_home::media::StreamSession;
using smart_home::protocol::MediaPacket;

static int g_failures = 0;
static void expect(bool cond, const char *msg) {
    if (cond) {
        std::cout << "  [ok]   " << msg << std::endl;
    } else {
        std::cout << "  [FAIL] " << msg << std::endl;
        ++g_failures;
    }
}

int main(int argc, char *argv[]) {
    std::cout << "=== Integration test begin ===" << std::endl;

    // ============ Part 1：Mock 源（纯 C++ 数据路径） ============
    {
        StreamSession session(std::unique_ptr<MediaSource>(new MockMediaSource()));
        expect(session.start("mock://camera0"), "mock: start returns true");

        MediaReassembler reassembler;
        const int kTotal = 100;
        int received = 0, keyCount = 0;
        bool ptsOk = true, keyOk = true, dataOk = true;
        int64_t lastPts = -1;

        // 模拟：服务端 nextSendPacket 拿字节 → 客户端 feed 进去 → nextPacket 还原
        while (received < kTotal) {
            std::vector<uint8_t> bytes;
            if (!session.nextSendPacket(bytes)) break;   // 服务端取一段字节
            reassembler.feed(bytes.data(), bytes.size()); // "网络"传给客户端
            MediaPacket pkt;
            while (reassembler.nextPacket(pkt) && received < kTotal) {
                if (pkt.pts <= lastPts) ptsOk = false;    // pts 应严格递增
                lastPts = pkt.pts;
                if (pkt.empty()) dataOk = false;          // 负载非空
                bool expectKey = (pkt.pts % 30 == 0);     // Mock 每 30 帧一个关键帧
                if (pkt.isKeyFrame() != expectKey) keyOk = false;
                if (pkt.isKeyFrame()) keyCount++;
                ++received;
            }
        }

        expect(received == kTotal, "mock: reassembled 100 packets");
        expect(ptsOk, "mock: pts strictly increasing");
        expect(dataOk, "mock: all packets non-empty");
        expect(keyOk, "mock: keyframes exactly every 30 packets");
        expect(keyCount == 4, "mock: 4 keyframes in 100 (pts 0/30/60/90)");
        session.stop();
    }

    // ============ Part 2：FFmpeg 源（真拉流，读本地文件） ============
    {
        std::string url = (argc > 1) ? argv[1] : "data/test_stream.mp4";
        StreamSession session(std::unique_ptr<MediaSource>(new FFmpegMediaSource()));
        expect(session.start(url), "ffmpeg: start returns true");

        MediaReassembler reassembler;
        const int kTotal = 50;
        int received = 0;
        bool dataOk = true;
        while (received < kTotal) {
            std::vector<uint8_t> bytes;
            if (!session.nextSendPacket(bytes)) break;
            reassembler.feed(bytes.data(), bytes.size());
            MediaPacket pkt;
            while (reassembler.nextPacket(pkt) && received < kTotal) {
                if (pkt.empty()) dataOk = false;
                ++received;
            }
        }

        expect(received == kTotal, "ffmpeg: reassembled 50 packets");
        expect(dataOk, "ffmpeg: all packets non-empty");
        session.stop();
    }

    std::cout << "=== Integration test end ===" << std::endl;
    if (g_failures == 0) {
        std::cout << "integration test passed." << std::endl;
        return 0;
    }
    std::cout << "integration test FAILED: " << g_failures << " items." << std::endl;
    return 1;
}