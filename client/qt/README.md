# Qt 客户端

`SmartHomeClient` 是 Windows + Qt 5.14.2 MinGW 32 bit 图形客户端，登录后进入监控工作台。

## 构建

在 Qt Creator 中打开本目录的 `CMakeLists.txt`，选择 Desktop Qt 5.14.2 MinGW 32 bit 套件，构建
`SmartHomeClient`。命令行构建示例：

```powershell
cmake -S client/qt -B client/qt/build -G Ninja `
  -DCMAKE_PREFIX_PATH=D:/QT/5.14.2/mingw73_32
cmake --build client/qt/build --target SmartHomeClient -j2
```

## 运行前配置

复制并编辑 `SmartHomeClient/conf/cameras.conf.example`（真实文件
`cameras.local.conf` 已被 Git 忽略），填写本机可访问的摄像头 RTSP/Web 地址、账号和 FFmpeg 路径。
真实凭据只能保存在本机配置，不能提交到仓库。

客户端默认连接 `8.163.52.40:7777`。部署到其它服务器时通过环境变量覆盖：

```powershell
$env:SMARTHOME_SERVER_IP = "服务器地址"
$env:SMARTHOME_SERVER_PORT = "7777"
.\SmartHomeClient.exe
```

实时预览和默认云台控制由 Windows 客户端直连局域网摄像头；只有设置
`ptzTransport=server` 或 `SMARTHOME_PTZ_TRANSPORT=server` 时才走服务端转发。
服务器要访问 `192.168.2.x` 摄像头，必须先具备 VPN、专线或端口映射路由。

## 登录后的操作

- **设备数据**：请求当前用户的设备列表并显示在线/离线状态；登录成功后也会自动刷新一次。
- **录像查询**：在工作台选择服务端设备，或进入数据页选择设备和时间范围后查询录像元数据。
- **回放**：查询后双击记录或点击“回放选中录像”。服务端文件需要映射到本机，并设置
  `SMARTHOME_RECORD_ROOT`；客户端不会假装提供不存在的远程文件下载。
- **开始/停止录像**：先建立同一 TCP 连接上的服务端流，再发录像控制请求；服务端需要 FFmpeg、
  摄像头网络路由和可写 `video_path`。
- **云台**：选择球机后等待“云台已就绪”，按住八方向键，松开即停止；枪机按钮保持禁用。

所有网络/协议错误会同时显示在底部状态栏和左侧“最近事件”面板，便于区分“无数据”和“请求失败”。
