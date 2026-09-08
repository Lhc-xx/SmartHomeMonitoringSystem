# 智能家居监控系统

面向小组协同开发的 C/S 智能家居监控系统。各成员模块已经合入统一工作树，
代码和离线测试覆盖 MVP 主链路；真实服务器、数据库和摄像头联调仍需在目标环境执行：

```text
Qt 注册/登录 → Server + TLV + MySQL → 设备列表 → 录像查询
   → Qt 本机 RTSP 预览 → 云台八方向控制 → 录像切片 + 元数据索引 + 回放
```

> 摄像头位于客户端可访问的局域网时，Qt 默认直连摄像头 Web API；只有显式设置
> `SMARTHOME_PTZ_TRANSPORT=server` 且服务器与摄像头可路由、已配置
> `ptz_allowed_hosts` 时，云台请求才经服务器转发。

> Qt 客户端默认连接 `127.0.0.1:7777`，不会把某台开发机公网地址写入程序。
> 需要连接远程服务器时，在启动客户端的同一个 PowerShell 会话中设置：
> `$env:SMARTHOME_SERVER_IP = "<server-ip>"` 和
> `$env:SMARTHOME_SERVER_PORT = "7777"`。端口只接受 1–65535。

## 功能状态

| 模块 | 状态 |
| --- | --- |
| 服务器 Reactor + ThreadPool + 配置 + 日志（A） | 代码完成；需 Linux 依赖验证 |
| TLV 协议 + 半包/粘包/非法长度（B） | 代码与离线测试完成 |
| MySQL 用户/设备/录像表 + 注册/登录/设备/录像查询（B） | 代码完成；需 MySQL 联调 |
| FFmpeg 拉流 + 可选服务器转发 + Qt 解码显示（C） | 代码完成；需 FFmpeg/摄像头联调 |
| 云台八方向控制（Qt 直连；同网段部署可选服务器转发）（D） | 代码与离线映射测试完成 |
| 录像真 MPEG-TS 切片 + records 索引 + 回放（C） | 代码完成；远程回放需共享文件路径或文件传输 |

## 依赖

- **服务器（Ubuntu 22.04）**：CMake ≥ 3.16、g++、log4cpp、libmysqlclient、OpenSSL、FFmpeg（libavformat/avcodec/avutil）、libcurl、cJSON。
- **Qt 客户端（Windows，Qt 5.14.2）**：Qt5 Widgets/Network；真 FFmpeg 解码需 MinGW 的 FFmpeg 开发库（`-DWITH_QT_FFMPEG=ON`），否则用 MockDecoder。

## 目录结构

```text
SmartHomeMonitoringSystem/
├── CMakeLists.txt                 # 顶层构建入口（cmake -S . -B build）
├── .gitignore                     # 忽略编译产物、日志、录像、本机配置
├── .github/workflows/ci.yml       # CI：push/PR 到 dev-integration、master 自动构建+测试
├── README.md                      # 项目说明与快速开始
├── LICENSE
├── 监控系统项目分工计划.md
├── common/                        # 客户端与服务器共享代码【B 主导】
│   ├── include/common/            # 通用类型、错误码、时间/字符串工具
│   ├── include/protocol/          # TLV 协议头、消息类型、序列化（跨端只定义一份）
│   ├── src/                       # 公共实现
│   └── tests/                     # 公共模块测试
├── server/                        # C++11 服务器【A 主导】
│   ├── conf/server.conf         # 脱敏配置（直接使用，禁止填真实密码）
│   ├── include/                   # 服务器头文件
│   ├── src/                       # 网络、配置、日志、数据库、媒体等实现
│   ├── log/                       # 运行日志（仅 .gitkeep）
│   ├── data/                      # 录像/临时数据（仅 .gitkeep）
│   ├── tests/                     # 服务器测试
│   ├── Makefile                   # 最小本地构建入口
│   └── CMakeLists.txt
├── client/
│   ├── linux/                     # Linux 基础客户端/联调客户端【A/D 联调】
│   │   ├── include/  src/  conf/  tests/
│   │   └── CMakeLists.txt
│   └── qt/                        # Windows Qt 图形客户端【B/D 界面，C 解码，A 网络层】
│       ├── include/  src/  forms/  resources/  tests/
│       ├── conf/client.conf
│       └── CMakeLists.txt         # 接入 Qt 后补充 find_package(Qt5 ...)
├── database/                      # SQL、迁移和种子数据【B】
│   ├── schema/                    # 完整表结构 SQL
│   ├── migrations/                # 按版本递增的结构变更 SQL
│   └── seed/                      # 脱敏初始化数据
├── docs/                          # 协议、数据库、设计、开发和测试文档
│   ├── protocol/                  # TLV 协议、消息类型、错误码、请求响应示例
│   ├── database/                  # 表结构、索引、字段约束
│   ├── design/                    # 架构图、模块职责、时序图
│   ├── development/               # 构建、分支、提交、调试说明
│   └── testing/                   # 测试用例、联调记录、验收报告
├── scripts/                       # 构建、测试、格式化等辅助脚本
├── third_party/                   # 第三方依赖说明或源码（默认不提交构建产物）
└── build/                         # 本地构建输出，不提交 Git
```

## 构建与运行

### 使用 CMake（推荐）

```bash
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

运行最小测试：

```bash
./build/server/tests/server_test
```

运行服务器：

```bash
./build/server/smart_home_server
```

运行 Linux 客户端（另开一个终端）：

```bash
./build/client/linux/smart_home_linux_client
```

Windows 下生成器不同，运行目标可能位于 `build/Debug/` 或 `build/Release/`。

### 使用 Makefile

服务器目录保留了一个不依赖第三方库的最小 Makefile：

```bash
make -C server test
```

## 协同开发约定

- `master`：稳定演示版本；禁止直接开发。
- `dev-integration`：日常集成和联调分支。
- `dev-lhc`、`dev-lqw`、`dev-pyj`、`dev-xgq`：个人功能分支。
- 提交信息建议使用 `feat:`、`fix:`、`test:`、`docs:` 前缀。
- 禁止提交真实密码、token、摄像头账号、地址和密钥；配置文件直接提交脱敏的 `server.conf` / `client.conf`。
- 公共协议和数据结构先更新 `common/` 与 `docs/protocol/`，再分别实现服务器和客户端。

## 验收状态（对照分工计划「六天最终验收」）

- [ ] Server 可读取配置、写日志并稳定启动（本机缺少 libcurl/cJSON/MySQL 开发库）
- [x] TLV 可处理正常包、半包、粘包和非法长度
- [ ] Qt 注册、登录和设备列表可用（离线协议/服务层测试通过，需真实服务端验证）
- [ ] 至少一路实时流经过 Server 到 Qt 显示（需目标 Linux FFmpeg 与摄像头）
- [ ] 云台八方向请求经过 Server 转发（离线参数映射通过，需目标环境 HTTP 联调）
- [ ] 录像文件、数据库索引和回放结果对应（远程文件访问方式需部署确认）
- [x] 服务器、Qt 客户端、Linux C 测试客户端有运行说明
- [x] 有协议文档、数据库 SQL、测试报告和演示脚本

详细测试记录见 [`docs/testing/README.md`](docs/testing/README.md)。
