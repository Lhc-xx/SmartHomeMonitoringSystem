# B Member Delivery Completion Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 完成 B 成员注册、登录、设备列表和录像查询的可交付闭环，并提供可复现测试与文档。

**Architecture:** 保留现有 Qt `UI → UserService → ClientProtocol → TcpClient` 和服务端 `Reactor → Handler → Service → MySQLClient` 分层。只在 B 数据页增加录像时间条件，在客户端断线时清理认证状态，并补强 B 的数据库/资源测试与文档，不修改 A/C/D 核心模块。

**Tech Stack:** C++11、Qt 5.14.2 Widgets/Network/Test、CMake 3.16、MySQL、OpenSSL、TLV 大端协议。

## Global Constraints

- 不修改 Reactor、ThreadPool、Connection 生命周期、FFmpeg、RingBuffer、HTTP、JSON 和云台实现。
- 所有新增 C++、SQL 和关键 CMake 逻辑添加详细中文注释。
- 不提交 Qt Creator `.user` 文件。
- Qt 使用 Desktop Qt 5.14.2 MinGW 32 bit；代码保持 C++11。
- 真实录像生成和播放不属于 B；B 只验证 `records` 元数据索引与查询。

---

### Task 1: Qt 认证状态与录像查询条件

**Files:**
- Create: `client/qt/SmartHomeClient/tests/UserServiceTest.cpp`
- Modify: `client/qt/SmartHomeClient/service/UserService.cpp`
- Modify: `client/qt/SmartHomeClient/MainWindow.h`
- Modify: `client/qt/SmartHomeClient/MainWindow.cpp`
- Modify: `client/qt/SmartHomeClient/CMakeLists.txt`

**Interfaces:**
- Consumes: `UserService::loginUser`、`UserService::requestDeviceList`、`UserService::requestRecordQuery` 和 `TcpClient` 信号。
- Produces: 断线后失效的内存会话；对象名为 `recordStartEdit`、`recordEndEdit`、`allTimeCheckBox` 的录像查询控件。

- [x] **Step 1: 写断线状态 RED 测试**

使用本地 `QTcpServer` 接收登录请求并返回合法登录 TLV，确认登录成功后主动断开，再调用设备列表请求：

```cpp
QSignalSpy requestFailedSpy(&service, &UserService::requestFailed);
peer->disconnectFromHost();
QTRY_COMPARE(disconnectedSpy.count(), 1);
service.requestDeviceList();
QCOMPARE(requestFailedSpy.takeFirst().at(0).toString(),
         QStringLiteral("获取设备列表失败：请先成功登录。"));
```

- [x] **Step 2: 运行测试并确认失败原因**

Run: `cmake --build <qt-build> --target UserServiceTest && ctest --test-dir <qt-build> -R UserServiceTest --output-on-failure`

Expected: FAIL，旧实现断线后仍保留 `m_userId/m_token`，错误信息来自发送失败而不是“请先成功登录”。

- [x] **Step 3: 最小修复断线状态**

在 `UserService::onDisconnected()` 清理认证和协议状态：

```cpp
void UserService::onDisconnected()
{
    const bool hadPendingRequest = m_pending != PendingRequest::None;
    if (hadPendingRequest) {
        failPending(QStringLiteral("与服务器的连接已断开。"));
    }
    m_userId = 0;
    m_token.clear();
    m_receiveBuffer.clear();
}
```

- [x] **Step 4: 增加录像时间控件和校验**

在数据页加入“全部时间”、开始时间和结束时间；全部时间勾选时发送两个空字符串，否则发送统一格式：

```cpp
const QString format = QStringLiteral("yyyy-MM-dd HH:mm:ss");
const QString startTime = m_allTimeCheckBox->isChecked()
    ? QString() : m_recordStartEdit->dateTime().toString(format);
const QString endTime = m_allTimeCheckBox->isChecked()
    ? QString() : m_recordEndEdit->dateTime().toString(format);
if (!startTime.isEmpty() && startTime > endTime) {
    m_dataStatus->setText(QStringLiteral("开始时间不能晚于结束时间。"));
    return;
}
```

- [x] **Step 5: 验证 Qt 测试恢复 GREEN**

Run: `ctest --test-dir <qt-build> --output-on-failure`

Expected: `ClientProtocolTest` 与 `UserServiceTest` 全部 PASS。

- [x] **Step 6: 提交 Task 1**

```bash
git add client/qt/SmartHomeClient
git commit -m "fix(b): finalize Qt auth state and record filters"
```

### Task 2: 设备与录像服务端集成测试补强

**Files:**
- Modify: `server/tests/resource_handler_test.cc`
- Modify: `server/tests/user_service_test.cc`

**Interfaces:**
- Consumes: `AuthHandler`、`ResourceHandler`、`DeviceService`、`RecordService`、`users/devices/records/user_sessions`。
- Produces: 登录会话、设备归属、录像时间过滤和越权隔离的可重复数据库测试。

- [x] **Step 1: 增加资源查询测试数据和断言**

向测试用户设备插入两条虚构录像元数据，一条在查询范围内、一条在范围外：

```cpp
mysql.execute("INSERT INTO records(device_id,file_path,start_time,end_time) VALUES(" +
              std::to_string(deviceId) +
              ",'./test-data/in-range.mp4','2026-09-01 10:00:00','2026-09-01 10:10:00')");
```

断言指定时间查询只返回范围内记录、其他用户设备返回空列表、错误 token 返回 `UNAUTHORIZED`、开始时间晚于结束时间返回 `INVALID_PARAMETER`。

- [x] **Step 2: 执行数据库测试**

Run: `./build/server/tests/resource_handler_test && ./build/server/tests/user_service_test`

Expected: 有 MySQL 环境时全部打印 `[PASS]`；没有开发库或配置时记录 `BLOCKED_BY_ENV`，不修改代码绕过。

- [x] **Step 3: 提交 Task 2**

```bash
git add server/tests/resource_handler_test.cc server/tests/user_service_test.cc
git commit -m "test(b): cover record filtering and authorization"
```

### Task 3: 数据库测试入口与演示数据

**Files:**
- Create: `database/seed/b_demo_data.sql.example`
- Modify: `server/tests/CMakeLists.txt`
- Modify: `database/README.md`

**Interfaces:**
- Consumes: 仓库外的 `server/conf/server.conf` 和已初始化的 `smarthome` 数据库。
- Produces: `SMARTHOME_ENABLE_DB_INTEGRATION_TESTS` CMake 选项以及设备/录像示例。

- [x] **Step 1: 为数据库测试增加显式 CTest 开关**

```cmake
option(SMARTHOME_ENABLE_DB_INTEGRATION_TESTS
       "注册需要真实 MySQL 的 B 成员集成测试" OFF)
if(SMARTHOME_ENABLE_DB_INTEGRATION_TESTS)
    add_test(NAME mysql_test COMMAND mysql_test)
    add_test(NAME user_service_test COMMAND user_service_test)
    add_test(NAME auth_handler_test COMMAND auth_handler_test)
    add_test(NAME resource_handler_test COMMAND resource_handler_test)
    set_tests_properties(mysql_test user_service_test auth_handler_test resource_handler_test
        PROPERTIES WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
endif()
```

- [x] **Step 2: 添加 SQL 样例**

样例引用 `demo_user` 和真实设备、`stream_url` 及录像相对路径，通过 `INSERT ... SELECT` 绑定已有测试用户。

- [x] **Step 3: 更新数据库说明**

说明初始化、加载样例、查询四张 B 表和清理测试数据的命令。

- [x] **Step 4: 提交 Task 3**

```bash
git add server/tests/CMakeLists.txt database/README.md database/seed/b_demo_data.sql.example
git commit -m "test(b): document database integration workflow"
```

### Task 4: 协议文档、交付报告与全量验证

**Files:**
- Modify: `docs/protocol/README.md`
- Create: `B_MEMBER_DELIVERY_REPORT.md`
- Modify: `docs/superpowers/plans/2026-09-06-b-member-delivery-completion.md`

**Interfaces:**
- Consumes: 最终代码、构建日志和测试输出。
- Produces: TLV/错误码/安全说明、PASS/BLOCKED_BY_ENV 证据和两分钟面试介绍。

- [x] **Step 1: 编写完整协议文档**

记录 12 字节 Header：`Type(uint16)`、`Version(uint16)`、`Length(uint32)`、`RequestId(uint32)`，以及注册、登录、设备、录像 request/response payload 顺序、1 MiB 上限和错误码。

- [x] **Step 2: Common 独立构建和测试**

Run:

```bash
cmake -S common -B build-common -DBUILD_TESTING=ON
cmake --build build-common -j2
ctest --test-dir build-common --output-on-failure
```

Expected: 五个 Common 测试全部 PASS。

- [x] **Step 3: Qt 5.14.2 MinGW32 重新构建和测试**

Run:

```powershell
cmake -S client/qt/SmartHomeClient -B build-qt-b-final -G Ninja `
  -DCMAKE_BUILD_TYPE=Debug `
  -DCMAKE_PREFIX_PATH=D:/QT/5.14.2/mingw73_32
cmake --build build-qt-b-final -j2
$env:PATH='D:\QT\5.14.2\mingw73_32\bin;D:\QT\Tools\mingw730_32\bin;' + $env:PATH
ctest --test-dir build-qt-b-final --output-on-failure
```

Expected: SmartHomeClient 构建成功，Qt 测试全部 PASS。

- [x] **Step 4: Server 配置与构建检查**

Run: `cmake -S . -B build -DBUILD_SERVER=ON && cmake --build build -j2`

Expected: Linux/MySQL 开发环境构建成功；当前 Windows 若缺 `mysqlclient` 则报告 `BLOCKED_BY_ENV`，不删除依赖或增加绕过代码。

- [x] **Step 5: 生成交付报告并做边界审查**

报告包括范围、架构图、已完成模块、TLV/大端/请求 ID/PBKDF2/token/Qt Model-View 技术点、逐项测试结果、风险和两分钟介绍。运行：

```bash
git diff --check
git diff --name-only origin/dev-integration...HEAD
git status --short
```

Expected: 没有 A/C/D 核心文件改动；`.user` 和旧 Qt 骨架保持未跟踪且不加入提交。

- [x] **Step 6: 提交 Task 4**

```bash
git add docs/protocol/README.md B_MEMBER_DELIVERY_REPORT.md docs/superpowers/plans/2026-09-06-b-member-delivery-completion.md
git commit -m "docs(b): finalize protocol and delivery report"
```
