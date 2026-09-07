# frp 内网穿透：把局域网摄像头接到云端服务器

摄像头在私网（192.168.2.x），云端服务器在公网（119.29.153.97），两者隔着 NAT。
用 frp 把摄像头的 RTSP(554) / 网页(80) 端口反向映射到云端本地端口，云端访问
`127.0.0.1:<remotePort>` 即等于访问摄像头。

## 拓扑

```
摄像头(192.168.2.100 枪机 / 192.168.2.160 球机)
        ↑ 局域网
本地机器(frpc, 与摄像头同网段)
        ↑ 公网隧道
云端服务器(119.29.153.97, frps)
        ↑ 127.0.0.1:8554/8555/8080/8081
SmartHome 服务器 / 客户端
```

## 1. 云端（119.29.153.97）跑 frps

1. 下载 frp：https://github.com/fatedier/frp/releases （选 linux_amd64）
2. 解压后放 `frps`，用本目录 `frps.toml`（把 `auth.token` 改成自己的随机串）
3. 启动：`./frps -c frps.toml`
4. 防火墙放行：`7000`（frp 控制）以及 `8554`、`8555`、`8080`、`8081`（映射端口）

## 2. 本地（与摄像头同网段）跑 frpc

1. 下载 frp（Windows 选 windows_amd64）
2. 复制 `frpc.toml.example` 为 `frpc.toml`，确认 `localIP` 是摄像头真实地址、
   `auth.token` 与云端一致
3. 启动：`frpc.exe -c frpc.toml`

## 3. 打通后的地址（云端视角）

| 摄像头 | RTSP | 网页 |
| --- | --- | --- |
| 枪机 | `rtsp://admin:admin@127.0.0.1:8554/live/chn=0` | `http://127.0.0.1:8080` |
| 球机 | `rtsp://admin:admin@127.0.0.1:8555/live/chn=0` | `http://127.0.0.1:8081` |

## 4. 项目里怎么用

- 服务器转发模式：客户端 `cameras.local.conf` 的 `rtspUrl/webUrl` 填上面的云端地址，
  并设 `SMARTHOME_USE_SERVER_STREAM=1`。
- 直连模式：Qt 客户端在本地网段时仍用 `192.168.2.x` 私网地址。

> 注意：服务器拉 RTSP 已固定走 TCP（`rtsp_transport=tcp`），确保能过 frp 的 TCP 隧道。
