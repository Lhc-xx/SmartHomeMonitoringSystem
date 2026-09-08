# 全成员交付收尾 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or **superpowers:executing-plans** to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 在不引入新业务协议、不启动服务器和摄像头的前提下，完成分工计划中各成员已有模块的交付收尾，修复已经能够由静态检查或单元测试证明的问题，并给出真实部署依赖的边界说明。

**Architecture:** 保留现有 A/C/D 的 Reactor、媒体、录像和云台实现，以及 B 的 common 协议、认证、数据库和 Qt Model/View 结构。新增的改动只放在边界层：Qt 服务器端点配置、服务端请求体严格解析、交付文档与测试，不改变线程池、连接生命周期或媒体编解码架构。

**Tech Stack:** C++11、Qt 5.14.2 Widgets/Network/Test、CMake、MinGW 32 bit；服务端仍按 Linux 依赖（MySQL、libcurl/cJSON、FFmpeg）审计。

## Global Constraints

- 不修改 `server` 以外成员的核心架构；不改 common 协议字段语义。
- 不启动服务端，不进行真实 TCP、RTSP 或云台硬件测试。
- 所有本次新增代码和测试添加详细中文注释。
- 不读取、打印或提交包含真实摄像头凭据的本地配置文件。
- 不暂存用户已有的 `.user`、临时协议示例等未跟踪文件。

---

## Task 1: 移除 Qt 客户端硬编码部署地址

**Files:** `client/qt/SmartHomeClient/network/ServerEndpoint.h/.cpp`、`client/qt/SmartHomeClient/MainWindow.cpp`、`client/qt/SmartHomeClient/tests/ServerEndpointTest.cpp`、Qt CMake。

- [x] 先添加覆盖默认值、环境变量覆盖和非法端口回退的测试，并确认在辅助类不存在时测试不能通过。
- [x] 实现 `resolveServerEndpoint()`：默认使用 `127.0.0.1:7777`，允许 `SMARTHOME_SERVER_IP`、`SMARTHOME_SERVER_PORT` 覆盖，端口仅接受 1–65535。
- [x] 让 `MainWindow` 使用该配置，删除源码中的固定公网 IP；保留部署时通过环境变量指向远程 ECS 的能力。
- [x] 构建并运行 Qt 测试，确认新增测试和原有测试全部通过。

## Task 2: 严格校验服务端流媒体/录像控制请求体

**Files:** `server/include/stream_request.h`、`server/src/stream_request.cc`、`server/src/reactor.cc`、`server/tests/stream_request_test.cc`、server CMake。

- [x] 先写纯解析器测试：流地址 TLV 必须完整、录像设备 ID 必须恰好 8 字节、停止请求必须为空；确认缺少实现时测试失败。
- [x] 实现大端序解析和长度边界检查，拒绝截断、尾随垃圾和无法表示的请求体；空流地址继续表示 Mock 流。
- [x] 在 Reactor 的流开始、录像开始、流停止和录像停止分支接入解析器，将非法请求返回既有错误码，不创建媒体会话或录像任务。
- [x] 运行纯解析器测试；无法满足 server 完整链接时只记录依赖阻塞，不通过改代码绕过。

## Task 3: 交付文档与静态一致性

**Files:** `README.md`、`docs/testing/README.md`、`ALL_MEMBERS_DELIVERY_REPORT.md`。

- [x] 记录 Qt 远程服务器端点环境变量、默认本机地址和不启动真实联调的验证边界。
- [x] 记录服务端录像文件回放依赖共享路径/文件传输，避免把 Windows 本地播放器误描述为已完成远程文件传输。
- [x] 让 Qt 回放播放器在启动 FFmpeg 前检查录像路径存在、为普通文件且可读，并用离线测试覆盖服务端路径不可访问的失败提示。
- [x] 汇总 A/B/C/D 已完成模块、实际测试 PASS 项和 server 依赖 `BLOCKED_BY_ENV` 项。
- [x] 检查文档和源码中不出现真实摄像头账号、密码或私有地址。

## Task 4: 全量验证与交付状态

- [x] 清理本次生成的临时构建目录之外的项目文件，不影响用户未跟踪文件。
- [x] 使用 Qt 5.14.2 MinGW 32 bit 构建并运行 Qt CTest。
- [x] 使用同一 Qt 编译器构建 common 并运行 common CTest。
- [x] 配置 server 进行依赖审计；缺少 FFmpeg/libcurl/cJSON/MySQL 时记录 `BLOCKED_BY_ENV`，不修改代码规避。
- [x] 执行 `git diff --check`、范围扫描和 `git status`，确认只留下本次交付修改。
