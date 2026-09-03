# AGENT.md —— 个人开发说明

> 面向 AI 助手与本项目成员的协作文档，记录我的开发环境与个人分工。
> 项目：智能家居监控系统（C/S 架构，C++11，Ubuntu 22.04 + Qt 5.14.2 + FFmpeg 4.4）。

---

## 一、开发环境

### 1. 服务器端开发 —— VSCode 远程连接

服务器端代码统一在 Ubuntu 22.04 远程服务器上开发、编译、运行。

**远程连接信息（待填写）**

| 项 | 值 |
| --- | --- |
| 服务器 IP / 域名 | `<待填写>` |
| SSH 端口 | `22` |
| 登录用户名 | `<待填写>` |
| 远程代码路径 | `~/SmartHomeMonitoringSystem`（示例，按实际填写） |

连接方式：VSCode 安装 Remote-SSH 插件，用 `ssh 用户名@服务器IP -p 22` 连接后在远程打开项目目录。

| 项 | 说明 |
| --- | --- |
| 语言 / 标准 | C++11，g++ |
| 构建工具 | CMake（>= 3.16），`cmake -S . -B build` |
| 依赖库 | log4cpp、pthread（服务器端当前依赖） |
| 系统环境 | Ubuntu 22.04、MySQL、FFmpeg 4.4（供流媒体 / 数据库联调） |

推荐的 VSCode 扩展：Remote-SSH、C/C++（ms-vscode.cpptools）、CMake Tools、clangd。

已有配置（无需重复设置）：

- `.vscode/settings.json`：已配置 `cmake.buildDirectory` 为 `build`、打开时自动 configure。
- `.clangd`：指向 `build` 下的 `compile_commands.json`（顶层 `CMakeLists.txt` 已开启 `CMAKE_EXPORT_COMPILE_COMMANDS`），保证 clangd 能识别 `server/include` 等 target 专属头文件。

log4cpp 依赖注意：服务器代码依赖 log4cpp（头文件 `/usr/local/include/log4cpp`，库 `/usr/local/lib`）。若编译报“找不到动态库”，需在 `/etc/ld.so.conf` 写入默认库路径后执行 `sudo ldconfig`。

### 2. Qt 客户端开发 —— Qt Creator

Qt 图形客户端在 Windows 上使用 Qt Creator（Qt 5.14.2）开发。

- 本地工程路径：`client/qt`（Windows 下打开该目录的 `CMakeLists.txt` 或后续补充的 `.pro`）。
- Qt 版本 / 构建套件：Qt 5.14.2（MinGW 或 MSVC，按本机安装填写）。
- 我负责其中的网络层：TLV 收发、超时、断线重连、资源释放（界面由 B/D、解码由 C 负责）。
- Qt 客户端默认不参与构建，待 Qt 5.14.2 装好后用 `-DBUILD_QT_CLIENT=ON` 打开，并在 `client/qt/CMakeLists.txt` 补充 Qt 工具链。

---

## 二、我的角色：A（服务器架构与网络）

### 职责总览

| 范围 | 任务 |
| --- | --- |
| Linux 服务端 | 配置、日志、Reactor、ThreadPool、任务分发、连接生命周期、流会话管理 |
| Qt / Linux C 客户端网络层 | TLV 收发、超时、断线重连、资源释放 |
| 交付结果 | Server 可启动、稳定运行；客户端可正常连接、重连、退出 |
| 个人验收标准 | 服务器能启动、处理并发连接、分发任务，正确处理断线和退出 |

### 我主导 / 参与的模块

- `server/`：服务器工程（A 主导）—— `src/`（网络、配置、日志、数据库、媒体）、`include/`、`conf/`、`tests/`。
- `client/linux/`：Linux 基础 / 联调客户端（A/D 联调）。
- `client/qt/`：Qt 客户端中的网络层（A）。
- `common/include/protocol/`：跨端 TLV 协议头 / 消息类型 —— 需配合 B 冻结协议定义。

### 6 天任务要点（我的部分）

| 天 | 后端 | 客户端 | 联调 |
| --- | --- | --- | --- |
| 第 1 天 基础统一 | 整理 Server 工程、配置、日志、启动入口 | Qt/Linux C TCP 网络骨架 | 连接、断开、日志验证 |
| 第 2 天 注册与基础服务 | 接入 Reactor + ThreadPool、任务分发 | Qt 异步请求、超时、重连 | 注册消息进入任务队列 |
| 第 3 天 登录与设备列表 | 登录状态机、会话超时、未登录拦截 | Qt/Linux C 登录状态处理 | 登录与设备请求串联 |
| 第 4 天 实时流与云台 | 流会话启动/停止、连接绑定、超时回收 | Qt 流接收和播放状态 | 一路流连接管理 |
| 第 5 天 录像与回放 | 录像开关、任务绑定、文件创建/关闭、目录检查 | Qt 录像按钮和状态 | 重复开启、停止、异常退出 |
| 第 6 天 集成与交付 | 线程、socket、日志、重启、部署检查 | Qt 断线、关闭、重复操作修复 | 全链路稳定性测试 |

### 协作关系

- A+B：Server、TLV、MySQL、认证和设备接口联调。
- A+D：HTTP、云台、Qt 网络状态和异常处理。
- 与 C 在流会话 / 媒体包转发边界处配合。

---

## 三、构建与运行（服务器端常用命令）

```bash
# 统一 CMake 构建（推荐）
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure

# 仅构建服务器
cmake -S . -B build -DBUILD_SERVER=ON
cmake --build build --target smart_home_server

# 运行最小测试
./build/server/tests/server_test

# 服务器 Makefile 最小入口（不依赖第三方库）
make -C server test
```

启动服务器（约定从项目根目录启动）：

```bash
./build/server/smart_home_server                 # 默认读 server/conf/server.conf
./build/server/smart_home_server /tmp/test.conf  # 指定配置文件
```

---

## 四、配置与日志约定

### 配置

- 配置模板：`server/conf/server.conf.example`，本地复制为 `server.conf` 使用。
- 默认项：`ip=127.0.0.1`、`port=7777`、`thread_num=4`、`task_num=10000`、`video_path=./data/`、`log_file=./log/server.log`。
- 配置读取由 `server/src/config.cc` 实现，`Config` 的取值方法均带默认值，加载失败不阻止启动。

### 日志（log4cpp 单例）

- 统一使用 `server/include/logger.h` 的宏：`LOG_DEBUG` / `LOG_INFO` / `LOG_WARN` / `LOG_ERROR`。
- 日志自动带 `文件名:行号 函数名`，双输出到控制台 + 日志文件（格式 `%d %c [%p] %m%n`）。
- 单例：首次 `Logger::getInstance(log_file)` 创建，进程退出前调用 `Logger::destroy()` 释放。
- 新业务模块建议按模块名创建 Category，方便按 `%c` 来源排查。

---

## 五、Git 协作约定

| 分支 | 负责人 | 用途 |
| --- | --- | --- |
| `master` | 全员维护 | 最终稳定演示版本，禁止直接开发 |
| `dev-lhc` | A（我） | 服务器架构、网络层、连接管理 |
| `dev-lqw` | B | TLV、MySQL、注册登录、设备/录像接口 |
| `dev-pyj` | C | FFmpeg、实时流、RingBuffer、录像 |
| `dev-xgq` | D | HTTP、JSON、云台、Qt 联调和测试 |
| `dev-integration` | 全员协作 | 每日集成和联调分支 |

流程：在 `dev-lhc` 开发 → 推送到 GitHub → 本地测试通过后提交 PR `dev-lhc → dev-integration` → 至少一人审核后合并 → 每天从 `dev-integration` 拉最新代码联调 → 第 6 天 `dev-integration → master`。

提交信息：使用 `feat:`、`fix:`、`test:`、`docs:` 前缀。

---

## 六、注意事项

- 禁止提交真实密码、token、摄像头账号/地址、密钥；配置用 `.example` 脱敏模板。
- 公共协议/数据结构先更新 `common/` 与 `docs/protocol/`，再由各端分别实现，避免两端各写一套不一致。
- 涉及 TLV、数据库、媒体包改动，需附协议说明/SQL/测试数据。
- 每天 18:00 前完成：提交个人分支 + 至少一次前后端联调 + 提交测试日志/截图/可复现命令 + 记录未解决问题与次日负责人。
- 当前阶段为骨架工程，业务代码增量实现到对应模块目录，避免堆在根目录。
