# D 成员交付报告（HTTP / 云台 / 质量交付）

## 1. 工作范围

libcurl + JSON + token、摄像头 HTTP 转发、云台八方向控制、Mock Server，以及 Qt 云台控制与状态反馈。

## 2. 已完成模块

### Qt 云台客户端
- `network/PtzClient.cpp`：`QNetworkAccessManager` 直连摄像头，`/api/ptz/baseConf` 能力探测 + `/api/ptz/control` 八方向控制（按下开始、释放/失焦停止），Basic 认证；`buildControlQuery`/`directionName` 可离线断言。

### 服务器云台转发（libcurl + token + JSON）
- `PtzHttpClient.cc`：libcurl 访问摄像头网页 API；`buildToken` 按参数字典序 `k=v&k=v` + secret + t 生成 **MD5 签名 token**（对照迅思维接口文档示例验证一致）；cJSON 解析响应；`probe`/`control`。控制请求将协议层 `direction/move` 映射为设备实际要求的 `channelId=1&value=<编码>&speed=<速度>`，并校验设备业务错误码。
- `PtzHandler.cc`：把 `PTZ_CONTROL_REQUEST`（cameraUrl + direction + move）转发到摄像头，返回 `errorCode`。
- 协议：新增 `PTZ_CONTROL_REQUEST=0x1801` / `RESPONSE=0x1802`，纳入登录鉴权。

### Mock Server
- 测试用 Python 模拟摄像头 HTTP 接口（`/api/ptz/baseConf`、`/api/ptz/control`），验证转发链路无需真实摄像头。

## 3. 测试结果

| 测试 | 结果 |
| --- | --- |
| `PtzClientTest`（Qt，离线假 HTTP 服务） | ✅ |
| 云台转发（WSL + libcurl + cJSON + mock 摄像头） | ✅ token 与文档示例一致、探测/控制/转发全通 |

## 4. 与其它成员的接口

- 向 A 注入 `PtzHandler`（reactor 把 `PTZ_CONTROL_REQUEST` 路由给它）。
- Qt 端 `PtzClient` 注入转发回调后经 `UserService` 发 TLV 到服务器（直连保留，测试不受影响）。

## 5. 已知事项

真实设备部署时，服务器转发仅适用于服务器与摄像头可路由且 `ptz_allowed_hosts` 已配置的环境；Qt 本机网段默认走直连。设备值映射集中在 `PtzHttpClient::deviceValue`，如固件变更只需调整该适配函数和对应离线测试。
