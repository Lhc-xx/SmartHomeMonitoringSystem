# A 成员交付报告（服务器架构与网络）

## 1. 工作范围

服务器配置、日志、Reactor + ThreadPool、连接生命周期、任务分发、流会话管理、录像开关绑定，以及 Linux 联调客户端。

## 2. 已完成模块

### 服务器基础
- `config.cc` / `logger.cc` / `main.cc`：读 `server.conf`、log4cpp 双输出（控制台+文件）、SIGINT/SIGTERM 优雅退出。
- `reactor.cc` + `thread_pool.cc`：epoll 事件循环 + 线程池任务分发。
- `connection.cc`：连接登录状态机（未登录/已登录/会话过期）。

### 连接生命周期与鉴权
- `session_policy.h`：纯函数定义「哪些消息需登录」「请求→响应类型映射」「未登录 UNAUTHORIZED 响应」，便于离线单测。
- 断线清理、指数退避重连、空闲连接回收、超时回收。

### 流会话与录像开关
- `STREAM_START/STOP`、`RECORD_START/STOP` 消息处理与连接绑定；流会话按 fd 存储、断开自动回收。
- 录像文件目录检查（`recorder.cc` 的 `ensureDirectoryExists`）。

### Linux 联调客户端
- `client/linux/src/main.cc`：真实 socket 跑通 推流→收帧→录像→停流，能从混合流里区分媒体帧与 TLV。

## 3. 测试结果

| 测试 | 结果 |
| --- | --- |
| `server_test` / `reactor_test` / `connection_session_test` / `session_policy_test` / `recorder_test` / `thread_pool_test` | ✅ |
| 全量 `ctest`（服务器 + 公共 + Linux 客户端） | ✅ 16/16 |

## 4. 与其它成员的接口

- 向 B 注入 `AuthHandler`/`ResourceHandler` 处理认证/资源请求；向 D 注入 `PtzHandler` 处理云台转发。
- 向 C 的 `StreamSession` 提供 sink 回调与录像开关入口；`RECORD_START/STOP` 现委托给 C 的 `TsRecorder` 分段录 TS。
