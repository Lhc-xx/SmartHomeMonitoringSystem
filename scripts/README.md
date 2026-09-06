# scripts 目录

放置构建、测试、格式化和部署运维脚本。

## 部署与运维（角色 A）

| 脚本 | 用途 |
| --- | --- |
| `deploy.sh` | 一键部署：依赖检查 + CMake 构建 + 初始化运行目录（server/log、server/data）+ 配置检查 |
| `server.sh` | 服务管理：start / stop / restart / status |

### 使用方法

约定从项目根目录执行：

```bash
# 部署（首次或代码更新后）
./scripts/deploy.sh

# 启动 / 停止 / 重启 / 查看状态
./scripts/server.sh start
./scripts/server.sh stop
./scripts/server.sh restart
./scripts/server.sh status
```

### 约定

- 服务器可执行文件由 CMake 输出到项目根目录（见 `server/CMakeLists.txt` 的 `RUNTIME_OUTPUT_DIRECTORY`）。
- 服务 PID 记录在 `server/server.pid`（已被 `.gitignore` 忽略）。
- 服务日志追加到 `server/log/server.log`。
- `server.sh stop` 发送 SIGTERM 触发服务器 `main.cc` 注册的优雅退出；超时未退出则强制 kill。
