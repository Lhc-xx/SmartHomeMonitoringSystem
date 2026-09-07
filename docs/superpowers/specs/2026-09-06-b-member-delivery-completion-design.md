# B 成员交付收尾设计

## 1. 目标

在最新 `dev-integration` 基线上完成 B 成员负责的协议、数据库、认证、设备列表和录像查询闭环，使“注册 → 登录 → 设备列表 → 录像查询”能够通过同一套 TLV 契约运行和验证。

## 2. 范围边界

本次只修改 B 成员拥有或直接支撑 B 验收的文件：

- `common/include/protocol`、`common/src`、`common/tests` 中的认证、设备和录像协议；
- `server` 中的 `AuthHandler`、`UserService`、`MySQLClient`、`DeviceService`、`RecordService`、`ResourceHandler` 及对应测试；
- `database/schema` 和测试数据说明；
- `client/qt/SmartHomeClient` 中的 `ClientProtocol`、客户端 `UserService`、登录界面、`DeviceModel`、`RecordModel` 和 B 数据页；
- B 成员协议、数据库、测试和交付文档。

不修改 Reactor、ThreadPool、Connection 生命周期、FFmpeg、RingBuffer、媒体转发、HTTP、JSON 和云台控制。最新集成分支已经由 A 将认证与资源请求路由到 B 的 Handler，本次仅验证该接口，不重写 A 的实现。

## 3. 采用方案

采用“在现有架构上最小补齐并回归”的方案：

1. 保留 `UI → UserService → ClientProtocol → TcpClient` 客户端依赖方向；
2. 保留 `Reactor → AuthHandler/ResourceHandler → Service → MySQLClient` 服务端依赖方向；
3. 不新增服务层或重新设计协议，仅补齐录像时间查询输入、客户端状态清理、测试覆盖和文档；
4. 录像测试只验证元数据索引与查询结果，实际录像文件生成和播放属于 C/D，不在 B 代码中实现。

没有采用的方案：

- 扩展并重写 A/C/D 模块：会越过成员边界并增加合并风险；
- 只写交付文档不补测试与界面：无法达到 B 的闭环验收标准。

## 4. 数据流

注册与登录：

```text
Qt LoginWidget
  → Qt UserService
  → ClientProtocol 大端 TLV
  → TcpClient
  → Reactor 已有消息路由
  → AuthHandler
  → UserService（PBKDF2 校验 / token 生成）
  → MySQL（users / user_sessions）
```

设备与录像：

```text
Qt 数据页
  → Qt UserService（userId + token）
  → Device/Record TLV
  → ResourceHandler（会话校验）
  → DeviceService / RecordService
  → MySQL（devices / records）
  → Qt DeviceModel / RecordModel
```

## 5. UI 与状态处理

- 登录成功后进入设备和录像元数据页，不增加视频播放控件；
- 设备列表显示名称、类型和在线状态；
- 录像查询增加可选开始时间和结束时间，统一发送 `yyyy-MM-dd HH:mm:ss`；
- 未选择设备、开始时间晚于结束时间、未登录、连接断开和协议响应错误均显示中文错误；
- 断线后清除客户端内存中的 `userId`、token、接收缓冲和待处理请求，避免旧会话被误用；
- token 永远不写入日志、界面、配置或仓库。

## 6. 数据库约定

- `users`：保存用户名、PBKDF2 密码摘要、随机 salt 和创建时间；
- `user_sessions`：保存 userId、token 的 SHA-512 摘要、过期和撤销时间；
- `devices`：保存用户归属、设备名称、类型、状态以及兼容字段；
- `records`：保存设备归属、录像路径、开始时间、结束时间和文件大小；
- 测试数据使用真实用户名、设备和路径。

## 7. 错误和安全策略

- TLV 头固定包含 Type、Version、Length 和 RequestId，全部数值使用网络大端序；
- 超过 1 MiB 的 TLV 正文直接拒绝；半包保留，粘包逐包消费；
- 非法时间范围返回参数错误，不执行数据库查询；
- 用户名和密码做长度检查，密码只经过 PBKDF2-HMAC-SHA256 派生后保存；
- 明文 token 只存在于登录响应和 Qt 进程内存，数据库只保存摘要；
- 数据库操作失败映射为协议错误码，不向客户端泄露 SQL 和数据库凭据。

## 8. 测试与验收

1. Common：协议正常包、半包、粘包、错误类型、错误版本、超长长度、登录/设备/录像字段边界；
2. Qt：请求编码、响应解析、录像时间范围、模型展示及客户端断线状态；
3. Server：MySQL connect/query/transaction、注册、重复注册、登录、错误密码、错误用户、token 摘要、设备归属、录像时间过滤和越权访问；
4. 构建：Common 和 Qt 在本机实际构建并运行 CTest；Server 仅在具备 Linux MySQL 开发库和 `server.conf` 的环境执行集成测试，否则明确记录 `BLOCKED_BY_ENV`；
5. 最终审查 `git diff`，确认没有修改 A/C/D 核心模块。

## 9. 完成标准

- B 的协议、数据库、认证、设备和录像查询代码有中文职责及关键设计注释；
- Qt 可输入录像查询时间并正确展示设备与录像元数据；
- 所有能够在当前环境执行的构建和测试为 PASS；
- 环境依赖测试有清晰的 `BLOCKED_BY_ENV` 说明和可复现命令；
- 生成 `B_MEMBER_DELIVERY_REPORT.md`，记录完成内容、架构、测试结果、风险和两分钟面试介绍。
