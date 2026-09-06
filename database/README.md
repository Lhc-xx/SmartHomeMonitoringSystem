# Database

数据库目录保存 B 成员负责的用户认证、设备和录像元数据结构，不保存媒体文件。

## 目录

- `schema/init.sql`：`users`、`devices`、`records`、兼容表 `recordings` 和 `user_sessions` 的完整初始化脚本；
- `migrations/`：按版本递增的结构变更脚本；
- `seed/b_demo_data.sql.example`：只包含虚构设备和录像路径的联调样例。

## 初始化

在 Linux/MySQL 环境中从项目根目录执行：

```bash
mysql -u root -p < database/schema/init.sql
```

密码由 MySQL 客户端交互读取，不能把 `-p密码` 写进脚本、终端截图或 Git 历史。

## 四张 B 核心表

- `users`：用户名、PBKDF2-HMAC-SHA256 密码摘要、每用户随机 salt；
- `user_sessions`：用户 ID、SHA-512 token 摘要、过期时间和撤销时间；
- `devices`：设备归属、名称、类型和在线状态；
- `records`：设备 ID、录像文件路径、开始时间、结束时间和可选文件大小。

数据库不保存明文密码和明文 token。`stream_url` 是兼容旧代码的可空字段，真实摄像头地址只能通过未跟踪配置或部署环境提供。

## 加载脱敏演示数据

先通过 Qt 注册一个用户名为 `demo_user` 的测试账号，再执行：

```bash
mysql -u root -p smarthome < database/seed/b_demo_data.sql.example
```

样例使用 `NULL stream_url` 和虚构相对录像路径，只用于验证设备列表及录像查询。它不表示磁盘上已经存在可播放文件。

## 运行 B 数据库测试

确保 `server/conf/server.conf` 只存在于部署机器且未被 Git 跟踪，然后执行：

```bash
cmake -S . -B build \
  -DBUILD_SERVER=ON \
  -DBUILD_TESTS=ON \
  -DSMARTHOME_ENABLE_DB_INTEGRATION_TESTS=ON
cmake --build build -j2 --target \
  mysql_test user_service_test auth_handler_test resource_handler_test
ctest --test-dir build -L b --output-on-failure
```

测试会使用专用用户名，并按 `user_sessions → records → devices → users` 的外键顺序清理数据，可重复运行。

## 验证数据

```sql
USE smarthome;
SELECT id, username, created_time FROM users ORDER BY id DESC;
SELECT id, user_id, device_name, device_type, status FROM devices ORDER BY id;
SELECT id, device_id, file_path, start_time, end_time FROM records ORDER BY start_time;
SELECT id, user_id, expires_at, revoked_at FROM user_sessions ORDER BY id DESC;
```

禁止提交真实账号、密码、token、摄像头地址、服务器配置或生产数据。
