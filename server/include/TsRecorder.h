#ifndef TS_RECORDER_H
#define TS_RECORDER_H

#include <string>
#include <vector>

namespace smart_home {

/*
 * TsRecorder：用 ffmpeg CLI 把一路流（RTSP/RTMP/本地文件）分段录制为真正的
 * MPEG-TS 文件（角色 C 维护，对应分工计划里的「TS 切片」）。
 *
 * 与旧的 Recorder（写序列化帧字节、不可回放）不同，本类直接让 ffmpeg 用
 * `-c copy -f segment -segment_format mpegts` 生成可回放的 TS 片段，避免在
 * 服务器里手写 MPEG-TS 封装。ffmpeg 以子进程方式运行，stop() 用 SIGTERM 优雅结束。
 */
class TsRecorder {
public:
    TsRecorder();
    ~TsRecorder();

    TsRecorder(const TsRecorder &) = delete;
    TsRecorder &operator=(const TsRecorder &) = delete;

    // 开始录制：url 为流地址，outputDir 为输出目录，segmentSec 每段目标时长。
    // 输出文件命名：<outputDir>/seg_%05d.ts。
    bool start(const std::string &url, const std::string &outputDir, int segmentSec = 10);

    // 停止录制（SIGTERM 优雅结束 ffmpeg 并等待退出）。幂等。
    void stop();

    bool isRecording() const;

    // 返回本次录制产出的 TS 文件绝对路径（按文件名排序，需在 stop() 之后调用）。
    std::vector<std::string> producedFiles() const;

private:
    int _pid;                 // ffmpeg 子进程 pid，-1 表示未运行
    std::string _outputDir;   // 输出目录
};

} // namespace smart_home

#endif // TS_RECORDER_H
