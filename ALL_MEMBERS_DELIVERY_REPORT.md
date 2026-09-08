# 全成员交付收尾报告

## 1. 工作范围

本次收尾以《监控系统项目分工计划.md》为验收基准，保留 A/B/C/D 已合入的
Reactor、协议认证、媒体流、录像和云台实现，只补充边界校验、部署配置和交付说明。
没有新增业务协议，也没有启动服务器、数据库、TCP、RTSP 或真实摄像头。

## 2. 系统架构

```text
Qt 客户端（登录、设备/录像 Model、实时预览、云台）
                         │
                         │ TCP / TLV（大端序、RequestId、长度上限）
                         ▼
服务端 Reactor + ThreadPool + Auth/Resource/Ptz Handler
                         │
                         ├── MySQL：users / user_sessions / devices / records
                         ├── FFmpeg：拉流、媒体转发、TS 切片
                         └── libcurl + cJSON：摄像头云台 HTTP 转发
```

## 3. 已完成模块

- A：配置、日志、Reactor、ThreadPool、连接状态、写队列和优雅清理。
- B：公共 TLV 编解码、认证/设备/录像协议、MySQLClient、PBKDF2 密码哈希、
  SHA-512 token 摘要、注册登录、设备列表和录像查询；Qt UserService 与 Model/View。
- C：MediaPacket、重组器、Ring/帧队列、StreamSession、FFmpeg 可选拉流、
  TS 分段录像和 Qt 播放占位/服务器转发播放器。
- D：libcurl/cJSON 云台转发、token/白名单校验、Qt 八方向按住移动/松开停止、
  摄像头参数映射和错误提示。
- 共同收尾：Qt 服务器地址改为安全默认值并支持环境变量；服务端严格拒绝
  截断、尾随垃圾和错误宽度的流/录像控制请求。

## 4. 核心技术点

- TLV：固定 `Type + Version + Length + RequestId` 头，value 使用长度前缀字段；
  所有长度采用大端序并限制为 1 MiB，支持半包/粘包拆分和非法包拒绝。
- 认证：注册生成随机 salt，使用 PBKDF2 保存密码哈希；登录成功生成 token，
  数据库只保存 SHA-512 token 摘要并通过 `user_sessions` 校验有效期和撤销状态。
- 网络：Qt `TcpClient` 只负责 socket；`UserService` 串行管理 requestId、超时、
  断线清理；服务器 Reactor 负责事件循环和输出队列。
- 媒体：服务器可选 FFmpeg/Mock 源，MediaPacket 经过重组后交给 Qt 解码器；
  TS 录像通过 `records` 保存文件索引和时间范围。
- 云台：八方向与停止动作经过统一参数映射；直连模式适合客户端与摄像头同网段，
  服务器转发模式要求 `ptz_allowed_hosts` 白名单和摄像头可路由。
- Model/View：Qt 设备和录像数据分别由 `DeviceModel`、`RecordModel` 呈现，
  UI 不直接操作 socket 或数据库。
- 回放边界：Qt 收到录像路径后，启动 FFmpeg 前会检查路径存在、为普通文件且可读，
  对服务端私有路径直接给出明确提示；跨主机回放仍需共享目录、映射盘或后续文件传输。

## 5. 测试结果

| 范围 | 结果 |
| --- | --- |
| Qt 5.14.2 MinGW32 构建 | PASS |
| Qt CTest（含 ServerEndpointTest、FilePlaybackPlayerTest 路径校验） | PASS，13/13 |
| Common CTest | PASS，5/5 |
| 流/录像请求体纯逻辑测试 | PASS，10/10 |
| Windows Server 完整配置 | BLOCKED_BY_ENV：缺少 libcurl/cJSON，后续还需 MySQL 客户端开发库 |
| MySQL 集成、真实 TCP、RTSP/FFmpeg 摄像头、云台硬件 | 未在本机执行，需目标环境联调 |

## 6. 交付前置条件

1. 在 Ubuntu 22.04 安装 server CMake 所需的 MySQL、OpenSSL、log4cpp、FFmpeg、
   libcurl 和 cJSON 开发包。
2. 初始化 `database/schema/init.sql`，填写服务器本地未提交的 `server.conf`。
3. Qt 远程部署时设置 `SMARTHOME_SERVER_IP`、`SMARTHOME_SERVER_PORT`；摄像头配置
   只放在被忽略的本地 `conf/cameras.local.conf`，不要提交账号和密码。
4. 若 Qt 在 Windows 上播放服务器返回的录像路径，客户端必须能访问该路径（共享目录、
   映射盘或后续文件传输方案）；当前代码没有虚构一个不存在的远程文件下载协议。

## 7. 面试介绍版本（约 2 分钟）

“我负责智能家居监控系统的协议、数据库、认证和 Qt 客户端数据链路，同时参与团队
集成收尾。协议层统一了大端序 TLV 头、消息类型、请求 ID、长度上限和错误码，处理了
半包、粘包和非法长度。服务端用 MySQLClient 完成用户、设备、录像和会话表，注册时
生成 salt 并用 PBKDF2 保存密码哈希，登录后生成 token，数据库只保存 token 摘要并校验
过期和撤销。Qt 侧通过 TcpClient、UserService 和 ClientProtocol 分层，完成注册、登录、
设备列表和录像查询的 Model/View 展示，并把网络错误、超时和断线清理反馈给界面。集成
时我还补了服务端流/录像请求体的严格大端序校验，以及 Qt 服务器地址的环境变量配置，
避免把开发机公网地址写死。媒体、FFmpeg 和云台由其他成员实现，我负责协议边界和接口
一致性，最后用 Qt/common 离线测试和服务端纯逻辑测试验证交付；真实服务器和摄像头联调
会在 Linux 依赖齐全后按演示脚本执行。”
