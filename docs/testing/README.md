# 测试报告

## 一、自动化测试（CTest）

### 服务器 + 公共模块（Ubuntu 22.04，`ctest --test-dir build`）

| 测试 | 覆盖 |
| --- | --- |
| `common_test` / `protocol_test` | 公共 TLV 编解码、工具链 |
| `auth_protocol_test` / `device_protocol_test` / `record_protocol_test` | 认证/设备/录像协议封包解包 |
| `server_test` | 服务器基础启动（Test 类） |
| `connection_session_test` | 连接登录状态机 |
| `session_policy_test` | 未登录拦截、请求/响应类型映射 |
| `recorder_test` | 录像文件写入器 + 目录检查 |
| `thread_pool_test` | 线程池 |
| `mock_media_source_test` | 假媒体源 |
| `reactor_test` | 连接/断线/重连/空闲回收/优雅停止 |
| `stream_session_test` | 流会话（拉流线程 + RingBuffer + 录像开关） |
| `ts_segmenter_test` | 录像切片器 |
| `integration_test` | Mock + FFmpeg 端到端（依赖 FFmpeg + 测试视频） |
| `linux_client_test` | Linux 联调客户端 |

仓库此前在目标 Ubuntu 环境记录过 16/16 通过（`integration_test` 依赖
`data/test_stream.mp4`）；本 Windows 工作树本轮未把该历史结果冒充为当前运行结果。

### Qt 客户端（Qt 5.14.2）

| 测试 | 覆盖 |
| --- | --- |
| `ClientProtocolTest` | 客户端 TLV 编解码、流/录像/云台控制请求 |
| `CameraConfigTest` | 摄像头配置校验 |
| `RtspPlayerTest` | ffmpeg 参数 + JPEG 拆包 |
| `VideoWidgetTest` | 视频控件状态 |
| `PtzClientTest` | 云台 URL/认证/开始停止参数及 `channelId/value/speed` 八方向映射 |
| `MonitoringDashboardTest` | 工作台四宫格/设备树/云台禁用状态 |
| `NetworkBehaviorTest` / `UserServiceTest` | 网络层与认证服务层 |
| `MainWindowTest` / `RegisterDialogTest` | 主窗口与注册对话框 |
| `FilePlaybackPlayerTest` | 录像回放 ffmpeg 参数、缺失/不可读本地路径的提前拒绝 |
| `ServerEndpointTest` | Qt 服务端默认地址、环境变量覆盖和非法端口回退 |

### 服务端纯逻辑测试

| 测试 | 覆盖 |
| --- | --- |
| `stream_request_test` | 流地址长度、录像 deviceId 大端序、停止请求空 value 校验 |

### 数据库集成测试（需 MySQL，`-DSMARTHOME_ENABLE_DB_INTEGRATION_TESTS=ON`）

`mysql_test`、`user_service_test`、`auth_handler_test`、`resource_handler_test`：连接/查询/事务、注册/登录/token、设备归属、录像查询。

## 二、本阶段手工验证记录

| 项 | 环境 | 结果 |
| --- | --- | --- |
| `FFmpegDecoder` 真解码（avcodec+sws_scale） | WSL + FFmpeg 4.4 | ✅ 解出 13 帧 320×240 |
| 云台转发（libcurl + MD5 token + cJSON + mock 摄像头） | WSL + libcurl + cJSON | ✅ token 与文档示例一致，探测/控制/转发全通 |
| `TsRecorder` 真 MPEG-TS 切片 | WSL + ffmpeg | ✅ 3 段 `format_name=mpegts`，`0x47` 同步字节 |
| 录像元数据 `addRecord`/`queryByDevice` | WSL + MySQL 8.0 | ✅ 写入 2 条并查询返回 |

## 三、环境说明

- 服务器只在 Linux 构建（epoll/log4cpp/mysqlclient/OpenSSL/FFmpeg/libcurl/cJSON）。
- Qt 客户端在 Windows Qt 5.14.2 构建；真 FFmpeg 解码需 `-DWITH_QT_FFMPEG=ON` + FFmpeg 开发库。
- 数据库集成测试与录像元数据测试需要本地 MySQL，先执行 `database/schema/init.sql` 并按 `server/conf/server.conf` 建库建用户。

## 四、本次合并后 Windows 验证（2026-09-08）

| 范围 | 命令/结果 |
| --- | --- |
| Common 协议 | 独立根构建（关闭 Server/Linux/Qt）后 CTest：5/5 PASS |
| Qt 5.14.2 MinGW32 | `SmartHomeClient` 构建：PASS；CTest：13/13 PASS（含回放路径校验） |
| Qt 服务端端点配置 | 新增 `ServerEndpointTest`，Qt CTest：13/13 PASS |
| 服务端流/录像请求体 | 独立 MinGW 纯逻辑编译运行：10/10 PASS |
| Qt 本机摄像头配置 | 构建后复制到可执行文件同级 `conf`：PASS（未输出配置内容） |
| Windows Server | configure：`BLOCKED_BY_ENV`，缺少 libcurl/cJSON（服务端还需要 MySQL 客户端库）；未修改 CMake 绕过 |
| 真实 Server/TCP/摄像头 | 本轮未启动、未访问 |
