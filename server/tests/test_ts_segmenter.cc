// 录像切片 + 索引

#include <fstream>
#include <iostream>

#include "media/mock_media_source.h"
#include "media/ts_segmenter.h"
#include "protocol/media_packet.h"

using smart_home::media::MediaSource;
using smart_home::media::MockMediaSource;
using smart_home::media::TsSegmenter;
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
    std::cout << "=== TsSegmenter test begin ===" << std::endl;

    MockMediaSource src;                 // 关键帧在 pts 0/30/60/90...
    TsSegmenter seg(1.0 / 25.0);         // 25fps：每帧 0.04 秒

    // 1) 启动，目标每段 1 秒（= 25 帧）
    expect(seg.start("/tmp/rec_test", 1), "start returns true");
    expect(seg.isRecording(), "isRecording == true");

    // 2) 写入 100 帧（pts 0~99）
    int written = 0;
    {
        MediaPacket p;
        while (written < 100 && src.readPacket(p)) {
            if (!seg.writePacket(p)) break;
            ++written;
        }
    }
    expect(written == 100, "write 100 packets");

    // 3) 切片数：关键帧 0/30/60/90 触发切段 → 共 4 段（0-29 / 30-59 / 60-89 / 90-99）
    std::vector<smart_home::media::SegmentInfo> segs = seg.segments();
    expect(segs.size() == 4, "4 segments generated");

    // 4) 验证索引字段
    if (segs.size() == 4) {
        expect(segs[0].startPts == 0  && segs[0].endPts == 29, "seg0 pts 0~29");
        expect(segs[1].startPts == 30 && segs[1].endPts == 59, "seg1 pts 30~59");
        expect(segs[2].startPts == 60 && segs[2].endPts == 89, "seg2 pts 60~89");
        expect(segs[3].startPts == 90 && segs[3].endPts == 99, "seg3 pts 90~99");
        expect(segs[0].durationSec == 1, "seg0 duration 1 sec");
        expect(segs[0].filePath.find("seg_0000.ts") != std::string::npos,
               "seg0 file name is seg_0000.ts");
    }

    // 5) 文件确实存在且非空
    {
        bool filesOk = true;
        for (size_t i = 0; i < segs.size(); ++i) {
            std::ifstream f(segs[i].filePath, std::ios::binary);
            f.seekg(0, std::ios::end);
            if (!f.good() || f.tellg() <= 0) filesOk = false;
        }
        expect(filesOk, "all segment files exist and are non-empty");
    }

    // 6) 停止
    seg.stop();
    expect(!seg.isRecording(), "isRecording == false after stop");

    std::cout << "=== TsSegmenter test end ===" << std::endl;
    if (g_failures == 0) {
        std::cout << "ts_segmenter test passed." << std::endl;
        return 0;
    }
    std::cout << "ts_segmenter test FAILED: " << g_failures << " items." << std::endl;
    return 1;
}