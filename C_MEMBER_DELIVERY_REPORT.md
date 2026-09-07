# C 成员交付报告（FFmpeg / 实时流 / 录像）

## 1. 工作范围

FFmpeg 拉流、媒体包序列化、RingBuffer、实时转发、断流重连、TS 切片、录像回放，以及 Qt 端媒体包重组与解码显示。

## 2. 已完成模块

### 服务端媒体
- `media/media_source.h`：媒体源抽象接口；`MockMediaSource`（假源）与 `FFmpegMediaSource`（真 avformat 拉流）都实现它。
- `media/stream_session.cc`：拉流线程 + RingBuffer + 断流重连 + 录像开关（直推/解耦两种消费方式）。
- `TsRecorder`：`fork` 出 ffmpeg 子进程，`-c copy -f segment -segment_format mpegts` 分段录**真 MPEG-TS**（`seg_%05d.ts`），SIGTERM 优雅收尾。
- `media/ts_segmenter.cc`：录像切片器（关键帧对齐 + 时长，简化版，单测用）。

### 公共媒体基础
- `protocol/media_packet.h/.cc`：MediaPacket + 大端序列化（帧长前缀 + 魔数）。
- `common/ring_buffer.h` / `frame_queue.h` / `media_reassembler.h/.cc`：环形缓冲、解码帧队列、媒体包重组器。

### Qt 端解码与显示
- `ffmpeg_decoder.cc`：真 `avcodec_send_packet/receive_frame` + `sws_scale`（YUV→RGBA），纯 C++ 不依赖 Qt。
- `decode_session.cc` / `mock_decoder.cc`：解码会话骨架 + 假解码器。
- `ServerStreamPlayer`：服务器转发链路（重组 + 解码 → QImage，独立工作线程）。
- `FilePlaybackPlayer`：录像回放（ffmpeg.exe 本地文件 → JPEG 帧流）。

## 3. 测试结果

| 测试 | 结果 |
| --- | --- |
| `mock_media_source_test` / `stream_session_test` / `ts_segmenter_test` / `integration_test` | ✅ |
| `FFmpegDecoder` 真解码（WSL + FFmpeg 4.4） | ✅ 解出 13 帧 320×240 |
| `TsRecorder` 真 MPEG-TS 切片（WSL + ffmpeg） | ✅ 3 段 `format_name=mpegts` |

## 4. 与其它成员的接口

- 向 A 提供 `StreamSession`（start/stop/reconnect/录像开关）与 `TsRecorder`。
- 向 B 提供 `TsRecorder::producedFiles()`（录像文件路径），B 据此写 `records` 索引。
- 向 Qt 端提供 `MediaReassembler` + `FFmpegDecoder`/`MockDecoder` + `FilePlaybackPlayer`。
