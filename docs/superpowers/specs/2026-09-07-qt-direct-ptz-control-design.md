# Qt 球机本地直连云台控制设计

## 背景与根因

当前摄像头使用 `192.168.2.0/24` 局域网地址，Qt 客户端运行在同一局域网的 Windows 电脑上；云服务器不在该网段。客户端可以直连球机网页服务完成只读能力探测，但 `MainWindow` 会给 `MonitoringDashboard` 注入服务器转发回调，导致移动和停止请求改由 ECS 服务端执行。

服务端默认以空白 PTZ 主机白名单拒绝请求，且 ECS 无法路由到该局域网球机地址。因此，当前“探测成功但球机不移动”的根因是错误的控制传输路径，而不是方向键映射或摄像头能力接口。

## 已确认的交付范围

- 枪机保持原有行为：不启用云台控制。
- 球机仅提供八方向移动和停止。
- 本机局域网联调时，球机控制由 Qt 客户端直连摄像头网页 API。
- 保留服务器转发能力，供将来服务器与摄像头处于同一网络或已建立 VPN 路由的部署使用。
- 不修改 ECS 配置，不把实际摄像头地址、账号或密码提交到 Git。
- 不新增变倍、聚焦、光圈等未经确认的控制能力。

## 方案比较

### 方案 A（采用）：每台摄像头声明控制传输方式

在本地摄像头配置增加 `ptzTransport`，支持 `direct` 与 `server`：

- `direct`：`PtzClient` 直接调用选中球机的 `/api/ptz/control`。
- `server`：保留现有 `UserService -> TLV -> PtzHandler` 转发链路。

优点是配置与网络拓扑匹配，不依赖 ECS 能访问家庭局域网，同时不删除团队已有的服务器转发实现。默认值为 `direct`，使已有本地配置在升级后即可用于局域网联调。

### 方案 B：完全删除服务器转发

改动最少，但会丢失已有的服务端安全校验和将来跨网络部署能力，因此不采用。

### 方案 C：只修改 ECS 白名单

白名单不能解决 ECS 到 `192.168.x.x` 私网缺少路由的问题，还会把局域网地址写入服务器环境；不采用。

## 模块设计

### `CameraConfig`

增加 `ptzTransport` 字段，读取 INI 中同名配置项。允许值为 `direct`、`server`；缺省值为 `direct`。非法值必须使配置加载失败并返回中文原因，避免静默选择错误的控制路径。

### `MonitoringDashboard`

保存由 `MainWindow` 注入的服务器转发回调，但不再立即永久注入 `PtzClient`。选择球机时，根据该球机的 `ptzTransport` 决定：

- `direct`：清空 `PtzClient` 的转发回调，移动与停止直接访问球机。
- `server`：为 `PtzClient` 注入已保存的 `UserService::sendPtzControl` 回调。

选择枪机或切换设备时仍清空当前 PTZ 目标并禁用全部按键，防止上一台球机的状态残留。

### `PtzClient`

不改变既有 HTTP 请求格式：按下方向键发送 `direction=<方向>&move=start`；释放、离开、失焦或中心停止键发送 `direction=stop&move=stop`。继续要求能力探测成功后才允许控制。

## 数据流

```text
球机树节点
  -> MonitoringDashboard 读取 ptzTransport
  -> direct: PtzClient -> 球机网页 API
  -> server: PtzClient -> UserService -> TLV -> 服务端 PtzHandler
```

枪机节点不进入此数据流，所有 PTZ 按键保持禁用。

## 错误处理与验证

- 配置中 `ptzTransport` 非法时，加载失败并报告分组名与中文错误原因。
- `direct` 模式不受 ECS 白名单或云端路由影响；HTTP 非 2xx 时仍由 `PtzClient` 上报“云台控制请求失败”。
- `server` 模式保留现有 TLV 错误处理与服务端白名单保护。
- 自动化测试必须覆盖：默认 direct、显式 server、非法传输方式、枪机禁用、球机方向/停止请求。
- 实机验证只在用户主动点击球机方向键时进行；测试代码只使用本机假 HTTP 服务，不驱动真实云台。

## 非目标

- 不为枪机增加 PTZ 功能。
- 不修改 `server`、`common`、协议编号或数据库。
- 不尝试通过 ECS 访问用户局域网摄像头。
