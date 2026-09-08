# B成员负责内容

## 1. 工作范围

本报告以 B 成员负责的协议、数据库、认证和 Qt 数据链路为主，并记录本轮在用户授权下为最终联调补齐的跨模块边界：

- 公共 TLV Header、消息 ID、请求 ID、Length、错误码和大端编解码；
- MySQLClient 的连接、执行、查询和事务接口；
- `users`、`devices`、`records`、`user_sessions` 表；
- 用户注册、PBKDF2 密码摘要、登录验证和 token 会话；
- AuthHandler、DeviceService、RecordService、ResourceHandler；
- Qt 注册/登录、登录状态、DeviceModel、RecordModel 和录像时间查询条件；
- Qt 端摄像头配置、QProcess+FFmpeg 实时预览工作台和球机八方向控制适配；
- Common、Qt 和 Linux/MySQL 环境中的 B 自动化测试。

本轮没有重写 A/C/D 的核心架构；在用户明确授权下，仅对服务端流/录像请求的严格校验、Qt 工作台请求反馈和云台 HTTP 适配做了必要收尾。FFmpeg、RingBuffer、媒体转发、视频解码、HTTP/JSON 的既有实现仍按原模块边界保留。

## 2. 系统架构

```text
Qt 客户端
  LoginWidget / MonitoringDashboard / DeviceModel / RecordModel
             |
       Qt UserService
             |
       ClientProtocol
             |
         TcpClient
             |
            TCP
             |
       大端 TLV 协议
             |
          Reactor
             |
  AuthHandler / ResourceHandler
             |
  UserService / DeviceService / RecordService
             |
        MySQLClient
             |
           MySQL
 users / user_sessions / devices / records

摄像头预览链路：CameraConfig → RtspPlayer → 本机 ffmpeg.exe → JPEG → VideoWidget；
球机控制链路：设备树选择球机 → PtzClient → 摄像头 Web API（只在用户按住方向键时发送）。
```

Reactor 到 B Handler 的路由由 A 成员负责；B Handler 不直接处理 socket。录像查询只返回元数据，视频文件生成和播放由 C/D 模块负责。

## 3. 已完成模块

### 公共协议

- 12 字节 TLV Header：Type、Version、Length、RequestId；
- 注册、登录、设备列表、录像查询 request/response；
- 正常包、半包、粘包、未知类型、错误版本、尾随数据和超长 Length 处理；
- 单包 Payload 最大 1 MiB，避免恶意 Length 导致超大内存分配。

### 数据库与认证

- MySQL connect、execute、query、beginTransaction、commit、rollback；
- 注册参数检查、用户名查重和数据库唯一索引并发兜底；
- 16 字节随机 salt、PBKDF2-HMAC-SHA256、100000 次迭代；
- 登录使用同一参数重新派生密码摘要并进行常量时间比较；
- 登录成功生成 32 字节随机 token，数据库只保存 SHA-512 摘要；
- 会话写入使用事务，包含过期和撤销字段。

### 设备与录像

- 按用户 ID 查询设备，返回名称、类型和状态；
- 资源请求验证 userId 与 token 会话；
- 录像查询同时限制设备归属和开始/结束时间；
- Qt DeviceModel 和 RecordModel 展示设备及录像元数据；
- Qt 支持全部时间或指定起止时间查询，并阻止倒置时间范围。
- Qt 监控工作台提供四宫格、事件列表、设备树和八方向云台按钮；
- 摄像头账号、密码和 RTSP 地址只放在被忽略的本地 INI 配置中，不进入 Git。

### Qt 客户端

- QTcpSocket 统一封装在 TcpClient；
- UserService 使用 requestId 串行关联请求和响应；
- 登录 token 只保存在进程内存，不显示、不记录；
- 断线后清理 userId、token、接收缓冲和等待状态；
- 保留 A 新增的请求超时与断线重连行为；
- 登录界面、设备列表和录像查询数据页均可由 Qt Creator 直接构建。

## 4. 核心技术点

### TLV协议设计

Header 固定为 `Type:uint16 + Version:uint16 + Length:uint32 + RequestId:uint32`。Payload 按消息类型解释，字符串采用 `uint16 字节长度 + UTF-8 字节`，各端不发送 C++ 结构体内存。

### 粘包拆包处理

TCP 接收缓冲不足一包时不消费数据；完整一包到达后只移除该包，继续循环处理剩余粘包。未知 Type、错误 Version 和超过 1 MiB 的 Length 会被安全拒绝。

### 大端序与请求ID

所有数值字段手工按网络大端写入和读取，保证 Windows/Qt 与 Linux Server 字节一致。客户端为每次请求生成非零 RequestId，服务端响应原样回传，避免异步响应串线。

### 用户认证

注册时先验证参数、查询用户名、生成 salt、派生密码摘要并写入数据库。登录时读取摘要和 salt，重新派生后进行常量时间比较，避免明文密码保存。

### PBKDF2密码加密

采用 PBKDF2-HMAC-SHA256，随机 salt 为 16 字节，迭代次数为 100000，输出摘要为 32 字节。这里严格说是单向密码派生/哈希，而不是可逆加密。

### Token机制

登录成功生成 32 字节安全随机 token；客户端仅在内存中保存明文，数据库保存 SHA-512 摘要和 24 小时有效期。设备及录像请求必须携带匹配的 userId/token。

### Qt网络通信

TcpClient 只负责连接、发送、断开和原始收包；ClientProtocol 负责字节；UserService 负责编排、超时、错误映射和认证状态；界面不直接访问 socket。

### Model/View设计

DeviceModel 和 RecordModel 基于 `QAbstractListModel`，将协议结果转换为 Qt View 可消费的数据。UI 只触发请求并展示状态，数据对象不依赖播放组件。

### Qt实时预览与云台

每路 `RtspPlayer` 通过 `QProcess` 启动本机 FFmpeg，使用 RTSP over TCP 输出受限尺寸的 MJPEG 字节流；Qt 在事件循环中拆分 JPEG 起止标记并交给 `VideoWidget` 等比绘制。`PtzClient` 先只读探测 `/api/ptz/baseConf`，球机被选中且能力探测成功后，方向按钮按下发送开始请求、释放/失焦发送停止请求。

## 5. 测试结果

### PASS

| 环境 | 命令/范围 | 结果 |
|---|---|---|
| Qt 5.14.2 MinGW32 | Qt Creator Debug 构建（两套现有构建目录） | PASS |
| Qt 5.14.2 MinGW32 | 两套 Qt Creator 构建目录 CTest | 12/12 PASS（每套） |
| Qt 5.14.2 MinGW32 | 顶层 CMake、仅 Qt 客户端全新构建 | PASS |
| Qt 5.14.2 MinGW32 | 顶层 Qt 客户端离屏 CTest | 17/17 PASS |
| Windows 本机摄像头 | 两路 RTSP 单帧探测 | PASS（本机网络可达） |
| Windows 本机摄像头 | 两路 `/api/ptz/baseConf` 能力探测 | PASS（只读请求） |

数据库测试覆盖连接/查询/事务、正常注册、重复用户、非法参数、正确登录、错误密码、错误用户、token 非明文落库、设备归属、设备状态 `1 → online` 转换、录像时间过滤和错误 token。Qt 测试另外覆盖摄像头配置校验、JPEG 拆包、视频控件、云台请求、工作台和主窗口构造；本轮 17 项离屏测试全部通过。

### BLOCKED_BY_ENV 与联调风险

- 当前 Windows 服务端配置被本机 MSYS2 C++ 工具链阻断：`D:\msys64\ucrt64\bin\c++.exe` 无法通过 CMake 的最小编译探测，因此不能据此宣称 Windows 服务端构建通过；这属于 `BLOCKED_BY_ENV`，不是源码绕过。
- ECS 远端服务器无法直接访问 `192.168.2.100/192.168.2.160` 的私有网段；因此服务端拉流、服务端录像和远端 PTZ 只能在 VPN、FRP、端口映射或同网段路由建立后验证。本机直连 RTSP 与只读 PTZ 能力探测已经通过。
- 录像查询、回放、设备数据和开始录像入口已接入请求/响应链路；回放仍要求服务端返回的文件路径挂载到客户端 `SMARTHOME_RECORD_ROOT`，不会伪造远端文件。

## 6. 面试介绍版本

我负责智能家居监控系统的 B 模块，主要包括 TLV 协议、MySQL 数据访问、用户认证，以及 Qt 端的注册登录、设备列表和录像查询模型。协议层设计了固定 12 字节 Header，包含消息类型、版本、Payload 长度和请求 ID，所有整数都使用网络大端序；接收端能够处理半包、粘包、非法版本和超大 Length。认证方面，注册密码不会明文落库，而是使用随机 salt 和 PBKDF2-HMAC-SHA256 进行十万次派生；登录校验成功后生成随机 token，数据库只保存 token 的 SHA-512 摘要和有效期。服务端通过 AuthHandler 和 ResourceHandler 将协议请求交给 UserService、DeviceService、RecordService，再通过 MySQLClient 完成事务和查询。Qt 端保持 UI、业务、协议、网络四层分离，DeviceModel 和 RecordModel 使用 Qt Model/View 展示数据；登录后由 MonitoringDashboard 管理四路画面，RtspPlayer 通过本机 FFmpeg 把两路 RTSP 转成 JPEG，VideoWidget 负责绘制，PtzClient 负责球机能力探测和八方向按住/松开控制。我还补充了 Common、Qt 和服务器边界测试，覆盖注册、重复用户、错误密码、token 鉴权、设备归属、录像时间过滤、JPEG 拆包、流/录像请求体校验和工作台构造。在用户授权下，本轮同时修复了跨模块联调边界，但没有重写其他模块的核心架构。

## 7. 当前风险与交付建议

1. 使用脱敏演示数据执行一次 Qt 注册→登录→设备列表→录像查询全链路冒烟，并确认状态栏显示服务器响应；
2. C 生成真实录像文件后，只需将对应路径和时间写入 `records`，B 查询接口无需感知编码格式；
3. `server/conf/server.conf` 只能保留脱敏占位值，不要提交 Qt Creator `.user`、真实 RTSP 地址、账号、密码和 token；
4. 在服务器与摄像头网络打通后，再验证 `STREAM_START → RECORD_START → RECORD_STOP` 和八方向 PTZ；最后由团队从 `dev-integration` 向 `master` 提交最终 PR。
