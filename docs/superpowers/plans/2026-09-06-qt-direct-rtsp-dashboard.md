# Qt Direct RTSP Dashboard Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 在现有 Qt 5.14.2 客户端中完成登录后的深色监控工作台，直接显示枪机和球机 RTSP 画面，并提供安全的球机云台控制入口。

**Architecture:** 使用 Qt Widgets 构建工作台；每路 `RtspPlayer` 通过 `QProcess` 管理本机 `ffmpeg.exe`，将 JPEG 帧异步交给 `VideoWidget`。`PtzClient` 通过 Qt Network 访问球机 Web API，`MainWindow` 只负责登录切换和组件装配，既有 UserService/DeviceModel/RecordModel 保持原边界。

**Tech Stack:** C++11、Qt 5.14.2 Widgets/Network/Test、QProcess、QImage、QNetworkAccessManager、FFmpeg 命令行。

## Global Constraints

- 只修改 `client/qt/SmartHomeClient` 及其 Qt 测试和脱敏示例配置；不修改 `server`、`common`、Reactor、ThreadPool、连接生命周期、HTTP/JSON 服务端、FFmpeg 服务端媒体模块或 C/D 成员代码。
- 真实摄像头账号、密码和 RTSP 地址只存在本机 `cameras.local.conf`，不得写入源码、测试、文档或 Git。
- 保持 C++11、Qt 5.14.2 MinGW 32 位兼容；不得要求 Qt 直接链接当前未安装的 FFmpeg 开发 SDK。
- 不启动服务端、不在自动化测试中连接真实 TCP 或向真实摄像头发送 PTZ 动作。
- 新增类文件顶部写中文职责说明，关键函数写中文设计原因和线程/错误处理说明。

---

## 文件与接口总览

| 文件 | 动作 | 交付接口 |
| --- | --- | --- |
| `client/qt/SmartHomeClient/video/CameraConfig.h/.cpp` | 新建 | 读取本地配置并产生 `CameraConfig` |
| `client/qt/SmartHomeClient/video/RtspPlayer.h/.cpp` | 新建 | `start()`, `stop()`, `setSource()`, `frameReady(QImage)`, `stateChanged(QString)` |
| `client/qt/SmartHomeClient/video/VideoWidget.h/.cpp` | 修改 | `setChannelName()`, `setFrame()`, `setState()`，绘制画面和状态覆盖层 |
| `client/qt/SmartHomeClient/network/PtzClient.h/.cpp` | 新建 | `probe()`, `startMove(Direction)`, `stopMove()`, `ptzReady`, `errorOccurred` |
| `client/qt/SmartHomeClient/ui/MonitoringDashboard.h/.cpp` | 新建 | 四宫格、事件栏、设备树、PTZ 控件和工作台信号 |
| `client/qt/SmartHomeClient/MainWindow.h/.cpp` | 修改 | 登录成功后创建/显示 `MonitoringDashboard`，关闭时释放播放和 PTZ 资源 |
| `client/qt/SmartHomeClient/conf/cameras.conf.example` | 新建 | 脱敏枪机/球机配置模板 |
| `client/qt/SmartHomeClient/.gitignore` | 修改 | 忽略 `conf/*.local.conf` 与本地 FFmpeg 路径配置 |
| `client/qt/SmartHomeClient/CMakeLists.txt` | 修改 | 加入工作台、播放器、云台类和测试目标 |
| `client/qt/SmartHomeClient/tests/RtspPlayerTest.cpp` | 新建 | 命令构造和 JPEG 帧边界测试 |
| `client/qt/SmartHomeClient/tests/PtzClientTest.cpp` | 新建 | 假 HTTP 服务下的请求构造测试 |
| `client/qt/SmartHomeClient/tests/MonitoringDashboardTest.cpp` | 新建 | 四通道和控件状态测试 |

## Task 1: 摄像头本地配置和 FFmpeg 进程边界

**Files:**
- Create: `client/qt/SmartHomeClient/conf/cameras.conf.example`
- Create: `client/qt/SmartHomeClient/video/CameraConfig.h`
- Create: `client/qt/SmartHomeClient/video/CameraConfig.cpp`
- Create: `client/qt/SmartHomeClient/tests/CameraConfigTest.cpp`
- Modify: `client/qt/SmartHomeClient/.gitignore`
- Modify: `client/qt/SmartHomeClient/CMakeLists.txt`

**Interfaces:**
- `struct CameraConfig` 字段：`name`、`type`、`rtspUrl`、`webUrl`、`user`、`password`、`enabled`、`ffmpegPath`。
- `QList<CameraConfig> loadCameraConfigs(const QString &path, QString *errorMessage);`
- 默认仅声明 `gun` 和 `dome` 两个示例槽位；示例 URL 使用 `rtsp://user:password@camera-ip/live/chn=0`，不写真实凭据。

- [ ] **Step 1: 写入脱敏配置模板和忽略规则**

  在示例中说明真实配置复制到 `conf/cameras.local.conf`，并加入 `conf/*.local.conf`、`ffmpeg.local.ini` 忽略规则。配置加载器只读取本地文件或 `SMARTHOME_CAMERA_CONFIG` 环境变量。

- [ ] **Step 2: 扩展 CMake 源文件清单**

  为后续 `RtspPlayer`、`PtzClient` 和 `MonitoringDashboard` 预留源文件项；保持 `Qt5::Widgets Qt5::Network`，测试目标额外链接 `Qt5::Test`。

- [ ] **Step 3: 写配置解析测试并实现加载器**

  测试临时文件包含两个 camera group 时，断言名称、类型、URL、Web 地址和 enabled 值被正确读取；文件缺失、缺少 URL 或 URL 不是 `rtsp://` 时返回空列表并写入错误。加载器使用 `QSettings::IniFormat`，不打印 password 字段。

- [ ] **Step 4: 验证配置不含秘密**

  运行：`rg -n "rtsp://|password=" client/qt/SmartHomeClient/conf client/qt/SmartHomeClient/tests`

  预期：无输出；示例只出现占位符。

## Task 2: RtspPlayer 的测试先行与实现

**Files:**
- Create: `client/qt/SmartHomeClient/video/RtspPlayer.h`
- Create: `client/qt/SmartHomeClient/video/RtspPlayer.cpp`
- Create: `client/qt/SmartHomeClient/tests/RtspPlayerTest.cpp`

**Interfaces:**
- `explicit RtspPlayer(QObject *parent = nullptr);`
- `void setSource(const QString &url, const QString &ffmpegPath);`
- `void start(); void stop(); bool isRunning() const;`
- Signals: `void frameReady(const QImage &frame);`, `void stateChanged(const QString &state);`, `void errorOccurred(const QString &reason);`

- [ ] **Step 1: 写失败测试，锁定 FFmpeg 参数和 JPEG 拆包契约**

  测试要求：`buildArguments()` 必须包含 `-rtsp_transport tcp`、`-f image2pipe`、`-vcodec mjpeg`、`-an` 和输出 `-`；连续输入 `FFD8...FFD9` 两个 JPEG 只发出两帧；超过 8 MiB 的无效缓存被丢弃并发出错误。

- [ ] **Step 2: 运行测试确认失败**

  运行：`cmake --build <qt-build> --target RtspPlayerTest`

  预期：因类和目标尚未实现而失败。

- [ ] **Step 3: 实现最小异步播放器**

  `QProcess` 只在 Qt 事件循环中使用；stdout 追加到有上限的 `QByteArray`，按 JPEG SOI `0xFFD8` 和 EOI `0xFFD9` 提取完整帧，使用 `QImage::fromData` 解码后发信号。stderr 仅保存最近一条摘要，不把 URL 中的账号密码写入错误文本。

- [ ] **Step 4: 实现退出与重连**

  `stop()` 发送 terminate，超时后 kill 并清空缓存；进程异常退出时进入“重连中”，使用 1/2/4/8 秒退避且上限 8 秒重启。手动 stop 时不得自动重启。

- [ ] **Step 5: 运行播放器测试确认通过**

  运行：`ctest --test-dir <qt-build> -R RtspPlayerTest --output-on-failure`

  预期：`100% tests passed, 0 tests failed`；测试不启动真实 FFmpeg、不访问摄像头。

## Task 3: VideoWidget 画面控件

**Files:**
- Modify: `client/qt/SmartHomeClient/video/VideoWidget.h`
- Modify: `client/qt/SmartHomeClient/video/VideoWidget.cpp`
- Create: `client/qt/SmartHomeClient/tests/VideoWidgetTest.cpp`

**Interfaces:**
- `void setChannelName(const QString &name);`
- `void setFrame(const QImage &frame);`
- `void setState(const QString &state, const QString &detail = QString());`

- [ ] **Step 1: 写无帧/有帧/错误状态测试**

  创建控件后验证默认状态为“等待接入”；设置 2×2 红色 `QImage` 后 `hasFrame()` 为真；设置“重连中”后状态文本可读且不清除最后一帧。

- [ ] **Step 2: 实现深色绘制**

  `paintEvent` 绘制深色背景、等比缩放帧、频道角标、在线/重连状态和错误提示；只在 GUI 线程更新 `QImage`，不在控件中启动进程。

- [ ] **Step 3: 运行控件测试**

  运行：`ctest --test-dir <qt-build> -R VideoWidgetTest --output-on-failure`

  预期：测试通过且无窗口弹出要求。

## Task 4: PtzClient HTTP 适配层

**Files:**
- Create: `client/qt/SmartHomeClient/network/PtzClient.h`
- Create: `client/qt/SmartHomeClient/network/PtzClient.cpp`
- Create: `client/qt/SmartHomeClient/tests/PtzClientTest.cpp`

**Interfaces:**
- `enum class Direction { Up, Down, Left, Right, UpLeft, UpRight, DownLeft, DownRight };`
- `void setCamera(const QUrl &webUrl, const QString &user, const QString &password);`
- `void probe();`
- `void startMove(Direction direction); void stopMove();`
- Signals: `void ptzReady(bool supported);`, `void errorOccurred(const QString &reason);`

- [ ] **Step 1: 写假 HTTP 服务测试**

  用本地 `QTcpServer` 返回固定 JSON，验证 `probe()` 只请求 `/api/ptz/baseConf`，验证 `startMove(Up)` 和 `stopMove()` 使用 `/api/ptz/control` 且请求方法、动作和方向字段稳定；测试断言请求中不出现明文密码。

- [ ] **Step 2: 实现认证和只读能力探测**

  使用 `QNetworkAccessManager` 异步请求设备公开的认证/基础配置端点；token 只保存在对象内存，错误信号只发 HTTP 状态和脱敏原因。未收到成功能力响应前，所有动作请求直接拒绝。

- [ ] **Step 3: 实现按下开始、释放停止**

  方向按键仅在 `ptzReady(true)` 后发送开始动作；`stopMove()` 在释放、鼠标离开、窗口失焦和析构路径都调用。请求失败自动禁用动作并发送错误，不自动重试物理动作。

- [ ] **Step 4: 运行 PTZ 假服务测试**

  运行：`ctest --test-dir <qt-build> -R PtzClientTest --output-on-failure`

  预期：通过；全程只访问本机假 HTTP 服务。

## Task 5: MonitoringDashboard 四宫格工作台

**Files:**
- Create: `client/qt/SmartHomeClient/ui/MonitoringDashboard.h`
- Create: `client/qt/SmartHomeClient/ui/MonitoringDashboard.cpp`
- Create: `client/qt/SmartHomeClient/tests/MonitoringDashboardTest.cpp`

**Interfaces:**
- `void setCameraConfigs(const QList<CameraConfig> &configs);`
- `void setDevices(const QList<ClientProtocol::DeviceInfo> &devices);`
- `void startPreview(); void stopPreview();`
- Signal: `void eventLogged(const QString &message);`

- [ ] **Step 1: 写界面结构测试**

  测试查找四个 `VideoWidget`、枪机和球机节点、设备树、PTZ 八方向按钮以及状态栏；断言枪机节点被选中时 PTZ 按钮 disabled。

- [ ] **Step 2: 实现布局和样式**

  使用深色标题栏、顶部导航、左侧事件栏、中央 `QGridLayout` 四宫格、右侧设备树与云台面板；通道 01/02 绑定两路配置，通道 03/04 显示“等待接入”。不添加无数据的告警图片或虚假状态。

- [ ] **Step 3: 接入 RtspPlayer**

  每个启用配置创建一个播放器，连接 `frameReady` 到对应 `VideoWidget::setFrame`，连接状态/错误到控件和事件栏；工作台停止时停止并释放所有播放器。

- [ ] **Step 4: 接入 PtzClient 和设备模型**

  设备树选中球机时设置 PTZ 摄像头并启动只读探测；选中枪机时断开/停止 PTZ 并禁用动作。既有 `DeviceModel`、`RecordModel` 保留在工作台导航页，不复制协议解析。

- [ ] **Step 5: 运行工作台结构测试**

  运行：`ctest --test-dir <qt-build> -R MonitoringDashboardTest --output-on-failure`

  预期：通过；测试不启动 FFmpeg、不访问真实摄像头、不发送 PTZ 动作。

## Task 6: MainWindow 登录切换与 CMake 集成

**Files:**
- Modify: `client/qt/SmartHomeClient/MainWindow.h`
- Modify: `client/qt/SmartHomeClient/MainWindow.cpp`
- Modify: `client/qt/SmartHomeClient/CMakeLists.txt`

- [ ] **Step 1: 在 MainWindow 中持有工作台**

  增加 `MonitoringDashboard *m_dashboard`；登录成功后隐藏 `LoginWidget`、创建/显示工作台并传入当前用户设备数据；登录失败仍留在登录页。

- [ ] **Step 2: 处理关闭顺序**

  在析构和窗口关闭路径先调用 `m_dashboard->stopPreview()`，再销毁 TCP/业务对象，确保没有遗留 FFmpeg 子进程或网络请求。

- [ ] **Step 3: 更新 CMake 测试目标**

  将新增源文件加入主程序和对应测试，保持 `AUTOMOC`、`AUTOUIC`、`AUTORCC` 和 `Qt5::Widgets/Network/Test` 配置。

- [ ] **Step 4: 完成 Qt 全量构建**

  运行：

  ```powershell
  cmake -S client/qt/SmartHomeClient -B <qt-build> -G Ninja -DCMAKE_PREFIX_PATH="D:/QT/5.14.2/mingw73_32"
  cmake --build <qt-build> -j2
  ctest --test-dir <qt-build> --output-on-failure
  ```

  预期：主程序和全部 Qt 测试构建成功，CTest 0 个失败。

## Task 7: 本机配置与人工冒烟验收

**Files:**
- Create locally (ignored, never commit): `client/qt/SmartHomeClient/conf/cameras.local.conf`

- [ ] **Step 1: 填入本机配置**

  将用户提供的两路地址和本地 FFmpeg 路径写入忽略文件；终端输出、日志和截图不显示账号密码。

- [ ] **Step 2: 启动客户端验证登录后工作台**

  只启动 Qt 客户端并连接已运行的远端服务端；登录成功后检查四宫格、设备树、状态栏和两路播放进程。此步骤不修改服务端代码。

- [ ] **Step 3: 验证断流恢复**

  关闭一个 FFmpeg 子进程或暂时断开对应摄像头网络，确认对应频道显示“重连中”，另一频道和界面仍可用。

- [ ] **Step 4: 用户手动验证 PTZ**

  仅在用户明确点击球机方向按钮时验证按住运动、松开停止；先用小步长和低速，发现方向或停止异常立即关闭客户端。枪机选中时不应发送任何 PTZ 请求。

## 自检清单

- [ ] 每个设计模块均有对应任务和自动化测试。
- [ ] 计划中没有未定义的类型、路径或函数签名。
- [ ] 计划没有要求修改 server/common 或引入未安装 SDK。
- [ ] 测试命令均为离线/本地假服务，真实设备只出现在最后人工验收。
- [ ] 真实凭据只进入被忽略的本地配置文件。
