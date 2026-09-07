// test_ffmpeg_media_source.cc —— 第八步：FFmpeg 真拉流（用本地文件测试）

#include <iostream>
#include <string>

#include "media/ffmpeg_media_source.h"

using smart_home::media::FFmpegMediaSource;
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
    // 流地址从命令行来：./test_ffmpeg_media_source data/test_stream.mp4
    // 以后换成真地址也只要改参数，不动代码
    std::string url = (argc > 1) ? argv[1] : "data/test_stream.mp4";

    std::cout << "=== FFmpegMediaSource test begin ===" << std::endl;
    std::cout << "url: " << url << std::endl;

    FFmpegMediaSource src;

    // 1) 打开失败：不存在的文件
    expect(!src.open("/nonexistent/xx.mp4"), "open nonexistent file returns false");

    // 2) 打开成功
    expect(src.open(url), "open returns true");

    // 3) 读 30 个视频包，验证字段
    {
        int read = 0, keyCount = 0;
        bool streamOk = true, dataOk = true;
        while (read < 30) {
            MediaPacket p;
            if (!src.readPacket(p)) break;
            ++read;
            if (p.streamIndex != 0) streamOk = false;
            if (p.empty()) dataOk = false;
            if (p.isKeyFrame()) keyCount++;
        }
        expect(read == 30, "read 30 video packets");
        expect(streamOk, "all packets have streamIndex 0");
        expect(dataOk, "all packets have non-empty data");
        expect(keyCount > 0, "at least one keyframe among them");
    }

    // 4) close 后读不到
    {
        src.close();
        MediaPacket p;
        expect(!src.readPacket(p), "readPacket after close returns false");
    }

    // 5) reconnect 后又能读
    {
        expect(src.reconnect(), "reconnect returns true");
        MediaPacket p;
        expect(src.readPacket(p) && !p.empty(), "after reconnect, can read again");
    }

    std::cout << "=== FFmpegMediaSource test end ===" << std::endl;
    if (g_failures == 0) {
        std::cout << "ffmpeg_media_source test passed." << std::endl;
        return 0;
    }
    std::cout << "ffmpeg_media_source test FAILED: " << g_failures << " items." << std::endl;
    return 1;
}