# 构建说明

## 一、服务器（Ubuntu 22.04）

### 依赖安装

```bash
sudo apt-get install -y build-essential cmake pkg-config \
    liblog4cpp5-dev libmysqlclient-dev libssl-dev \
    libavformat-dev libavcodec-dev libavutil-dev \
    libcurl4-openssl-dev libcjson-dev ffmpeg
```

### 构建与测试

```bash
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

### 服务器常用开关

| 选项 | 默认 | 说明 |
| --- | --- | --- |
| `-DBUILD_SERVER=ON\|OFF` | ON | 是否构建服务器 |
| `-DBUILD_LINUX_CLIENT=ON\|OFF` | ON | 是否构建 Linux 联调客户端 |
| `-DBUILD_TESTS=ON\|OFF` | ON | 是否构建测试 |
| `-DWITH_FFMPEG=ON\|OFF` | ON | 是否把 `FFmpegMediaSource` 编入服务器（检测到 FFmpeg 才生效） |
| `-DSMARTHOME_ENABLE_DB_INTEGRATION_TESTS=ON\|OFF` | OFF | 是否把依赖 MySQL 的 B 集成测试注册进 CTest |

### 数据库（B 集成测试需要）

```bash
sudo apt-get install -y mysql-server
# 启动后执行 database/schema/init.sql，并按 server/conf/server.conf 建用户/库
```

录像（角色 C）依赖本机 `ffmpeg` 命令行（`TsRecorder` 用它分段录 MPEG-TS），服务器运行时需保证 `ffmpeg` 在 PATH 中。

## 二、Qt 客户端（Windows，Qt 5.14.2）

用 Qt Creator 打开 `client/qt/CMakeLists.txt`（会进入 `SmartHomeClient` 子目录）。

### 可选后端

| 选项 | 默认 | 说明 |
| --- | --- | --- |
| `-DWITH_QT_FFMPEG=ON` | OFF | 服务器转发链路用真 FFmpeg（avcodec/swscale）解码；需 MinGW 的 FFmpeg 开发库，否则回退 MockDecoder |
| `-DWITH_VLC=ON` | OFF | VLC（libvlc）播放后端；需 VLC SDK（`-DVLC_LIBRARY_DIR=<sdk/lib>`） |

### 运行时演示开关（环境变量）

| 变量 | 作用 |
| --- | --- |
| `SMARTHOME_SERVER_IP` / `SMARTHOME_SERVER_PORT` | 覆盖服务器地址/端口 |
| `SMARTHOME_CAMERA_CONFIG` | 摄像头本地配置路径（默认 `conf/cameras.local.conf`） |
| `SMARTHOME_USE_SERVER_STREAM=1` | 走「服务器转发 → FFmpeg 解码」链路（默认走 RtspPlayer 直连） |
| `SMARTHOME_STREAM_URL` | 指定服务器推流地址（空则取第一路启用摄像头的 RTSP） |

## 三、运行

```bash
# 部署（首次或代码更新后）
./scripts/deploy.sh

# 启动 / 停止 / 状态
./scripts/server.sh start
./scripts/server.sh stop
./scripts/server.sh status
```

服务器约定从项目根目录启动（读 `server/conf/server.conf`），录像输出到 `video_path`（默认 `./data/`）。
