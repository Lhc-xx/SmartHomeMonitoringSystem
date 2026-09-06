# B成员负责内容

## 1. 工作范围

B 成员负责协议、数据库、认证、设备列表和录像查询元数据闭环：

- 公共 TLV Header、消息 ID、请求 ID、Length、错误码和大端编解码；
- MySQLClient 的连接、执行、查询和事务接口；
- `users`、`devices`、`records`、`user_sessions` 表；
- 用户注册、PBKDF2 密码摘要、登录验证和 token 会话；
- AuthHandler、DeviceService、RecordService、ResourceHandler；
- Qt 注册/登录、登录状态、DeviceModel、RecordModel 和录像时间查询条件；
- Common、Qt 和 Linux/MySQL 环境中的 B 自动化测试。

明确未实现 Reactor、ThreadPool、Connection 生命周期、FFmpeg 拉流、RingBuffer、媒体转发、视频解码、HTTP、JSON 和云台控制。

## 2. 系统架构

```text
Qt 客户端
  LoginWidget / DeviceModel / RecordModel
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

## 5. 测试结果

### PASS

| 环境 | 命令/范围 | 结果 |
|---|---|---|
| Windows MinGW32 | Common 顶层独立构建与 CTest | 5/5 PASS |
| Qt 5.14.2 MinGW32 | SmartHomeClient 全量构建 | PASS |
| Qt 5.14.2 MinGW32 | ClientProtocol、网络行为、UserService、MainWindow | 4/4 PASS |
| Ubuntu 22.04 + MySQL | mysql、UserService、AuthHandler、ResourceHandler | 4/4 PASS |
| Ubuntu 22.04 + MySQL | 脱敏 `b_demo_data.sql.example` | PASS |

数据库测试覆盖连接/查询/事务、正常注册、重复用户、非法参数、正确登录、错误密码、错误用户、token 非明文落库、设备归属、录像时间过滤和错误 token。

### FAIL / 外部集成阻断

最新 `dev-integration` 的完整 `smart_home_server` 在 Ubuntu 链接阶段失败：`main.cc` 调用了 `Reactor::setResourceHandler(ResourceHandler*)`，但当前 A 模块 `reactor.cc` 缺少对应定义。B 的四个独立服务端测试均已构建并通过；该问题需要 A 在 Reactor 模块补齐后再执行最终 Qt→Server 全链路回归。

### BLOCKED_BY_ENV

- Windows 没有 MySQL Server 开发头文件/库，因此服务端不在 Windows 编译；已经在 Ubuntu/MySQL 环境完成 B 测试；
- 测试摄像头位于 `192.168.2.x` 私网，而当前开发机不在该网段，RTSP 访问属于 C 的联调环境阻断，不影响 B 元数据测试。

## 6. 面试介绍版本

我负责智能家居监控系统的 B 模块，主要包括 TLV 协议、MySQL 数据访问、用户认证，以及 Qt 端的注册登录、设备列表和录像查询模型。协议层设计了固定 12 字节 Header，包含消息类型、版本、Payload 长度和请求 ID，所有整数都使用网络大端序；接收端能够处理半包、粘包、非法版本和超大 Length。认证方面，注册密码不会明文落库，而是使用随机 salt 和 PBKDF2-HMAC-SHA256 进行十万次派生；登录校验成功后生成随机 token，数据库只保存 token 的 SHA-512 摘要和有效期。服务端通过 AuthHandler 和 ResourceHandler 将协议请求交给 UserService、DeviceService、RecordService，再通过 MySQLClient 完成事务和查询。Qt 端保持 UI、业务、协议、网络四层分离，DeviceModel 和 RecordModel 使用 Qt Model/View 展示数据。我还补充了 Common、Qt 和 MySQL 集成测试，覆盖注册、重复用户、错误密码、token 鉴权、设备归属和录像时间过滤。这样完成了从注册、登录到设备及录像元数据查询的 B 模块闭环，同时没有越界实现 Reactor、FFmpeg 或云台功能。

## 7. 当前风险与交付建议

1. A 需要补齐 `Reactor::setResourceHandler` 定义并重新构建整服；
2. A 修复后，使用脱敏演示数据执行一次 Qt 注册→登录→设备列表→录像查询；
3. C 生成真实录像文件后，只需将对应路径和时间写入 `records`，B 查询接口无需感知编码格式；
4. 不要提交 `server/conf/server.conf`、Qt Creator `.user`、真实 RTSP 地址、账号、密码和 token；
5. 合入前由 A/B 共同核对 Handler 路由和 requestId，随后再从 `dev-integration` 向 `master` 提交最终 PR。
