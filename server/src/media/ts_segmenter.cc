// 录像切片器实现

#include "media/ts_segmenter.h"

#include <cstdio>      // snprintf：拼文件名
#include <sys/stat.h>  // mkdir：创建目录

#include "protocol/media_packet.h"  // MediaPacketSerializer

namespace smart_home {
namespace media {

TsSegmenter::TsSegmenter(double timeBase)
    : _timeBase(timeBase),
      _segmentDurationSec(60),
      _recording(false),
      _nextIndex(0),
      _segmentStartPts(0),
      _lastPts(0) {}

TsSegmenter::~TsSegmenter() {
    stop();   // 对象销毁时收尾当前段
}

bool TsSegmenter::start(const std::string &dir, int64_t segmentDurationSec) {
    if (_recording) {
        return false;      // 已在录像
    }
    mkdir(dir.c_str(), 0755);   // 目录不存在就创建；已存在会失败但可忽略

    _dir = dir;
    _segmentDurationSec = segmentDurationSec;
    _nextIndex = 0;
    _segments.clear();
    _recording = true;
    return true;
}

void TsSegmenter::stop() {
    if (!_recording) {
        return;
    }
    if (_file.is_open()) {
        closeCurrentSegment(_lastPts);   // 收尾当前段，记录索引
    }
    _recording = false;
}

bool TsSegmenter::writePacket(const protocol::MediaPacket &pkt) {
    if (!_recording) {
        return false;
    }

    // 判断是否需要切新段：
    //  1) 还没开过段；2) 当前包是关键帧、且当前段已经够长
    bool needNew = !_file.is_open();
    if (!needNew && pkt.isKeyFrame()) {
        int64_t elapsedSec =
            static_cast<int64_t>((pkt.pts - _segmentStartPts) * _timeBase);
        if (elapsedSec >= _segmentDurationSec) {
            needNew = true;
        }
    }

    if (needNew) {
        if (_file.is_open()) {
            closeCurrentSegment(_lastPts);      // 结束旧段
        }
        if (!openNextSegment(pkt.pts)) {
            return false;                       // 开新段失败
        }
    }

    // 序列化这个包，写入当前段文件
    std::vector<uint8_t> bytes;
    if (!protocol::MediaPacketSerializer::encode(pkt, bytes)) {
        return false;
    }
    _file.write(reinterpret_cast<const char *>(bytes.data()), bytes.size());
    if (!_file.good()) {
        return false;
    }

    _lastPts = pkt.pts;
    return true;
}

bool TsSegmenter::openNextSegment(int64_t startPts) {
    char name[64];
    std::snprintf(name, sizeof(name), "seg_%04d.ts", _nextIndex);  // seg_0000.ts ...
    _currentPath = _dir + "/" + name;

    _file.open(_currentPath.c_str(), std::ios::binary | std::ios::trunc);
    if (!_file.is_open()) {
        return false;
    }

    _segmentStartPts = startPts;
    _lastPts = startPts;
    ++_nextIndex;
    return true;
}

void TsSegmenter::closeCurrentSegment(int64_t endPts) {
    _file.close();

    SegmentInfo info;
    info.fileIndex = static_cast<int>(_segments.size());
    info.filePath = _currentPath;
    info.startPts = _segmentStartPts;
    info.endPts = endPts;
    info.durationSec = static_cast<int64_t>((endPts - _segmentStartPts) * _timeBase);
    _segments.push_back(info);
}

}  // namespace media
}  // namespace smart_home