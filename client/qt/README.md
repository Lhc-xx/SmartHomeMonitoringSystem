# Qt 客户端

Qt 图形客户端位于 `SmartHomeClient/` 子目录，使用 Qt 5 Widgets/Network 和
Qt Designer UI。顶层 `client/qt/CMakeLists.txt` 只负责把该工程接入仓库构建。

## 本地构建

```powershell
cmake -S client/qt/SmartHomeClient -B build-qt -G "MinGW Makefiles"
cmake --build build-qt -j2
ctest --test-dir build-qt --output-on-failure
```

Qt 5.14.2 MinGW32 的 `bin` 目录需要位于运行测试时的 `PATH` 中。默认服务器端点
为 `127.0.0.1:7777`；远程联调前在同一 PowerShell 会话设置
`SMARTHOME_SERVER_IP` 和 `SMARTHOME_SERVER_PORT`。

## 运行时配置

- `conf/cameras.local.conf`：本机摄像头 RTSP/Web 配置，已被 Git 忽略，禁止提交真实账号、
  密码和地址。
- `SMARTHOME_USE_SERVER_STREAM=1`：显式选择服务端转发流；默认使用 Qt 本机 RTSP 播放器。
- `SMARTHOME_PTZ_TRANSPORT=server`：显式选择服务端云台转发；默认直连摄像头 Web API。

Qt 客户端不负责启动服务端或数据库；完整联调请先按根目录 README 和
`docs/development/build.md` 配置 Linux 服务端依赖。
