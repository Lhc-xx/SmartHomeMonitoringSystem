#ifndef SERVER_MEDIA_TS_SEGMENTER_H
#define SERVER_MEDIA_TS_SEGMENTER_H

// ============================================================================
// ts_segmenter.h —— 录像切片器（角色 C 维护）
//
// 把一路实时流按"关键帧对齐 + 时长"切成多段录像文件，并维护每段索引。
//
// 【与 B 的接口】segments() 返回的 SegmentInfo 列表 = 录像索引元数据，
//   B 拿它写数据库录像表、支持按时间查询（字段见结构体注释）。
// 【与 D 的接口】录像文件的路径 + 命名规则（seg_0000.ts...），
//   D 拿它做回放地址与文件存在性校验。
// 【与 A 的接口】录像开关由客户端触发，但本类只提供 start()/stop()。
//
// 注意：本步骤文件内容是简化版（写序列化帧字节），
//   真正的 MPEG-TS 封装后续用 FFmpeg muxer 替换写入部分。
// ============================================================================

#include <cstdint>
#include <fstream>
#include <string>
#include <vector>

#include "protocol/media_packet.h"

namespace smart_home {
namespace media {

// 一段录像切片的索引元数据（跨角色接口：B 写库、D 回放都用它）
struct SegmentInfo {
    int fileIndex;         // 切片序号，从 0 开始
    std::string filePath;  // 文件路径
    int64_t startPts;      // 开始时间戳（流 time_base 单位）
    int64_t endPts;        // 结束时间戳
    int64_t durationSec;   // 时长（秒）
};

class TsSegmenter {
public:
    // timeBase：每个 pts 单位对应多少秒。
    // 真实流从 FFmpeg 的 time_base 来（如 1/90000）；Mock 是 25fps → 1/25。
    explicit TsSegmenter(double timeBase = 1.0 / 25.0);
    ~TsSegmenter();

    // 开始录像：dir 为录像目录（不存在会创建），segmentDurationSec 为每段目标时长
    bool start(const std::string &dir, int64_t segmentDurationSec = 60);

    // 写入一个媒体包；内部自动处理"关键帧对齐切片"
    bool writePacket(const protocol::MediaPacket &pkt);

    // 停止录像，关闭当前文件
    void stop();

    bool isRecording() const { return _recording; }

    // 已生成的所有切片索引（留接口给 B/D）
    std::vector<SegmentInfo> segments() const { return _segments; }

private:
    bool openNextSegment(int64_t startPts);   // 开新段文件
    void closeCurrentSegment(int64_t endPts); // 关当前段并记录索引

    double _timeBase;              // pts → 秒 的换算系数
    int64_t _segmentDurationSec;   // 每段目标时长（秒）
    std::string _dir;              // 录像目录
    bool _recording;               // 是否在录像
    int _nextIndex;                // 下一个切片序号
    std::string _currentPath;      // 当前切片文件路径
    int64_t _segmentStartPts;      // 当前切片起始 pts
    int64_t _lastPts;              // 上一个写入包的 pts
    std::ofstream _file;           // 当前切片文件流
    std::vector<SegmentInfo> _segments;  // 已完成的切片索引
};

}  // namespace media
}  // namespace smart_home

#endif  // SERVER_MEDIA_TS_SEGMENTER_H