# 智能家居监控系统

这是一个面向小组协同开发的 C/S 智能家居监控系统基础工程骨架。
当前阶段只提供：

- 服务器、Linux 客户端、Qt 客户端、公共模块的目录结构；
- CMake 构建入口；
- 一个最小可运行的 `Test` 测试类；
- 脱敏配置示例、数据库脚本占位和协作文档入口。

网络层（Reactor + ThreadPool）已完成，MySQL / FFmpeg / Qt 界面待实现

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
│   ├── conf/server.conf.example   # 脱敏配置模板（本地复制为 server.conf）
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
│       ├── conf/client.conf.example
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
- 禁止提交真实密码、token、摄像头账号、地址和密钥；配置文件使用 `.example`。
- 公共协议和数据结构先更新 `common/` 与 `docs/protocol/`，再分别实现服务器和客户端。

## 当前最小验收标准

- 能配置并生成 CMake 工程；
- `Test` 类可被服务器测试程序调用；
- 测试输出 `Test passed.` 并返回 0；
- 三个主要模块均有清晰的后续代码放置位置。
