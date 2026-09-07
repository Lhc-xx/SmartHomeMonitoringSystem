# Qt 独立注册对话框 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans or superpowers:subagent-driven-development to implement this plan task-by-task.

**Goal:** 将登录页的注册入口改为独立、清晰的模态注册对话框，同时复用现有注册业务链路。

**Architecture:** 新增 `RegisterDialog`（`.h/.cpp/.ui`）承载注册表单和校验；`LoginWidget` 只保留登录处理并在按钮点击时创建对话框。对话框接收同一个 `UserService` 指针，异步等待现有 `registerSuccess/registerFailed` 信号，不改变协议和网络层。

**Tech Stack:** C++11、Qt 5.14.2 Widgets、现有 UserService、QtTest/QSignalSpy。

## Global Constraints

- 只修改 Qt 客户端界面与测试；不修改 server/common、TLV、TcpClient 或摄像头播放逻辑。
- 所有新增类和关键函数添加详细中文注释。

### Task 1: 注册对话框回归测试

**Files:**
- Create: `client/qt/SmartHomeClient/tests/RegisterDialogTest.cpp`
- Modify: `client/qt/SmartHomeClient/CMakeLists.txt`

- [ ] 写测试：验证对话框拥有用户名、密码、确认密码、提交和取消控件，并且初始可见。
- [ ] 写测试：空字段和密码不一致时状态文本变化，且未触发 `UserService::registerUser`（通过无网络的空服务场景验证界面层拒绝）。
- [ ] 运行 `RegisterDialogTest`，确认在对话框尚不存在时编译或测试失败。

### Task 2: 实现注册对话框

**Files:**
- Create: `client/qt/SmartHomeClient/ui/RegisterDialog.h`
- Create: `client/qt/SmartHomeClient/ui/RegisterDialog.cpp`
- Create: `client/qt/SmartHomeClient/ui/RegisterDialog.ui`
- Modify: `client/qt/SmartHomeClient/CMakeLists.txt`

- [ ] 使用 Qt Designer 控件 `usernameEdit/passwordEdit/confirmPasswordEdit/registerButton/cancelButton/statusLabel`。
- [ ] 在提交槽中校验空值和两次密码一致，再调用 `UserService::registerUser(username,password)`。
- [ ] 连接现有注册成功/失败信号；成功时发出注册用户名并接受对话框，失败时保留窗口并显示原因。
- [ ] 为按钮设置清晰的中文文案、模态窗口标题和最小尺寸。

### Task 3: 接入登录页

**Files:**
- Modify: `client/qt/SmartHomeClient/ui/LoginWidget.h`
- Modify: `client/qt/SmartHomeClient/ui/LoginWidget.cpp`
- Modify: `client/qt/SmartHomeClient/ui/LoginWidget.ui`

- [ ] 注册按钮文案改为“注册新账号”，点击时只打开 `RegisterDialog::exec()`。
- [ ] 移除登录页对注册结果信号的直接连接，避免重复提示。
- [ ] 对话框成功返回后回填用户名、清空密码并提示“注册成功，请登录”。

### Task 4: 构建和回归

**Files:**
- Modify: none

- [ ] 使用 `D:\QT\5.14.2\mingw73_32` 配置 Qt 工程。
- [ ] 运行 `cmake --build ... -j2`。
- [ ] 运行 `ctest --test-dir ... --output-on-failure`，期望所有测试通过。
- [ ] 运行 `git diff --check`，确认没有 server/common/database 改动。
