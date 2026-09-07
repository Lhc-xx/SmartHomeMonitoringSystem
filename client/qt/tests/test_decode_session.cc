#include <iostream>
#include <memory>

#include "decode_session.h"
#include "mock_decoder.h"
#include "protocol/media_packet.h"

using smart_home::client::DecodeSession;
using smart_home::client::Frame;
using smart_home::client::IDecoder;
using smart_home::client::MockDecoder;
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

static MediaPacket makePacket(int64_t pts) {
    MediaPacket p;
    p.streamIndex = 0;
    p.pts = pts;
    p.dts = pts;
    p.data = {1, 2, 3, 4};
    return p;
}

int main() {
    std::cout << "=== DecodeSession test begin ===" << std::endl;

    // 场景1：喂 10 包，取回 10 帧，pts 递增
    {
        DecodeSession session(std::unique_ptr<IDecoder>(new MockDecoder()));
        session.start();
        for (int i = 0; i < 10; ++i) {
            session.feed(makePacket(i));
        }
        int got = 0;
        bool ptsOk = true;
        int64_t last = -1;
        Frame f;
        while (got < 10 && session.nextFrame(f)) {
            if (f.pts <= last) ptsOk = false;
            last = f.pts;
            ++got;
        }
        expect(got == 10, "decoded and retrieved 10 frames");
        expect(ptsOk, "frame pts strictly increasing");
        session.stop();
    }

    // 场景2：新开会话，验证帧内容（宽高 64x48，R 分量随 pts 变）
    {
        DecodeSession session(std::unique_ptr<IDecoder>(new MockDecoder()));
        session.start();
        session.feed(makePacket(7));
        Frame f;
        bool got = session.nextFrame(f);
        expect(got, "retrieve one frame");
        expect(f.width == 64 && f.height == 48, "frame size 64x48");
        expect(f.rgba.size() == 64u * 48 * 4, "rgba size = w*h*4");
        // 只有取到帧（rgba 非空）才访问 rgba[0]，避免越界
        if (!f.rgba.empty()) {
            expect(f.rgba[0] == 7, "R channel == pts (mock color)");
        }
        session.stop();
    }

    // 场景3：stop 后取不到帧
    {
        DecodeSession session(std::unique_ptr<IDecoder>(new MockDecoder()));
        session.start();
        session.stop();
        expect(!session.isRunning(), "isRunning false after stop");
        Frame f;
        expect(!session.nextFrame(f), "nextFrame returns false after stop");
    }

    std::cout << "=== DecodeSession test end ===" << std::endl;
    if (g_failures == 0) {
        std::cout << "decode_session test passed." << std::endl;
        return 0;
    }
    std::cout << "decode_session test FAILED: " << g_failures << " items." << std::endl;
    return 1;
}