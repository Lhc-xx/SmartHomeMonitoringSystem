// test_mock_media_source.cc —— 第七步：假媒体源（接口 + Mock 行为）

#include <iostream>

#include "media/mock_media_source.h"

using smart_home::media::MockMediaSource;
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

int main() {
    std::cout << "=== MockMediaSource test begin ===" << std::endl;

    MockMediaSource src;   // 默认每 30 帧一个关键帧

    // 1) 未 open 就读 -> false
    {
        MediaPacket p;
        expect(!src.readPacket(p), "readPacket before open returns false");
    }

    // 2) open
    expect(src.open("mock://camera0"), "open returns true");

    // 3) 连续读 65 帧，验证字段
    {
        bool fieldsOk = true, ptsOk = true, keyOk = true, payloadOk = true;
        int keyCount = 0;
        for (int i = 0; i < 65; ++i) {
            MediaPacket p;
            if (!src.readPacket(p)) { fieldsOk = false; break; }
            if (p.streamIndex != 0) fieldsOk = false;
            if (p.pts != i || p.dts != i) ptsOk = false;
            if (p.empty()) payloadOk = false;

            bool isKey = p.isKeyFrame();
            if (isKey) keyCount++;
            // 关键帧应恰好在 0、30、60 出现
            if (isKey != (i % 30 == 0)) keyOk = false;
        }
               expect(fieldsOk, "all 65 packets have streamIndex 0");
        expect(payloadOk, "all 65 packets have non-empty data");
        expect(ptsOk, "pts/dts increment 0,1,2,...");
        expect(keyOk, "keyframe exactly every 30 frames");
        expect(keyCount == 3, "3 keyframes among 65 frames (0, 30, 60)");
        expect(src.frameCount() == 65, "frameCount() == 65 after reading");
    }

    // 4) close 后读不到
    {
        src.close();
        MediaPacket p;
        expect(!src.readPacket(p), "readPacket after close returns false");
    }

    // 5) reconnect 后重新从 0 开始
    {
        expect(src.reconnect(), "reconnect returns true");
        MediaPacket p;
        expect(src.readPacket(p) && p.pts == 0, "after reconnect, pts restarts from 0");
    }

    std::cout << "=== MockMediaSource test end ===" << std::endl;
    if (g_failures == 0) {
        std::cout << "mock_media_source test passed." << std::endl;
        return 0;
    }
    std::cout << "mock_media_source test FAILED: " << g_failures << " items." << std::endl;
    return 1;
}