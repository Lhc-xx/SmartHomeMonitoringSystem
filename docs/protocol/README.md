# Smart Home TLV 协议

本文档描述 B 成员负责的认证、设备列表和录像查询线协议。服务端公共协议使用 `std::vector<uint8_t>`，Qt 客户端使用 `QByteArray`，两端不能直接发送 C++ 结构体内存。

## 1. 外层消息结构

每条消息由固定 12 字节 Header 和可变 Payload 组成：

```text
+--------+---------+---------+------------+------------------+
| Type   | Version | Length  | RequestId  | Payload          |
| uint16 | uint16  | uint32  | uint32     | Length 字节      |
+--------+---------+---------+------------+------------------+
  0..1     2..3      4..7      8..11        12..
```

| 字段 | 作用 |
|---|---|
| `Type` | 消息 ID，区分注册、登录、设备列表和录像查询请求/响应 |
| `Version` | 当前固定为 `1`，其他版本返回 `UNSUPPORTED_VERSION` |
| `Length` | Payload 字节数，不包含 12 字节 Header，最大 `1 MiB` |
| `RequestId` | 客户端生成的非零请求标识，服务端响应原样返回 |
| `Payload` | 与 Type 对应的业务字段 |

所有整数使用网络大端序。所有字符串编码为 UTF-8，并使用以下 length-string：

```text
String := ByteLength(uint16, big-endian) + UTF8Bytes(ByteLength)
```

字符串长度按 UTF-8 字节数计算，不按 Unicode 字符数量计算。

## 2. 消息 ID

| Type | 名称 | 方向 |
|---:|---|---|
| `0x1001` | `REGISTER_REQUEST` | Client → Server |
| `0x1002` | `REGISTER_RESPONSE` | Server → Client |
| `0x1101` | `LOGIN_REQUEST` | Client → Server |
| `0x1102` | `LOGIN_RESPONSE` | Server → Client |
| `0x1201` | `DEVICE_LIST_REQUEST` | Client → Server |
| `0x1202` | `DEVICE_LIST_RESPONSE` | Server → Client |
| `0x1301` | `RECORD_QUERY_REQUEST` | Client → Server |
| `0x1302` | `RECORD_QUERY_RESPONSE` | Server → Client |

流媒体和云台消息由其他成员负责，不在本文档中重新定义。

## 3. 注册

### REGISTER_REQUEST Payload

```text
username:String
password:String
```

服务端业务层额外限制用户名为 1～64 字节，密码为 1～128 字节。

### REGISTER_RESPONSE Payload

```text
errorCode:int32
message:String
```

当前解码器兼容历史 4 字节响应，即只包含 `errorCode`；新服务端响应始终携带 message 字段。

## 4. 登录

### LOGIN_REQUEST Payload

```text
username:String
password:String
```

### LOGIN_RESPONSE Payload

```text
userId:uint64
token:String
errorCode:int32
```

登录成功要求 `userId != 0`、token 非空且 `errorCode == SUCCESS`。登录失败时不得返回有效 token。

## 5. 设备列表

### DEVICE_LIST_REQUEST Payload

```text
userId:uint64
token:String
```

### DEVICE_LIST_RESPONSE Payload

```text
errorCode:int32
count:uint16
repeat count times:
    id:uint64
    deviceName:String
    deviceType:String
    status:String
```

设备查询只能返回 `userId` 所属设备；空列表是合法成功响应。

## 6. 录像查询

### RECORD_QUERY_REQUEST Payload

```text
userId:uint64
token:String
deviceId:uint64
startTime:String
endTime:String
```

时间使用 `yyyy-MM-dd HH:mm:ss`。两个时间字符串同时为空表示查询全部时间；非空时必须满足开始时间不晚于结束时间。

### RECORD_QUERY_RESPONSE Payload

```text
errorCode:int32
count:uint16
repeat count times:
    id:uint64
    deviceId:uint64
    filePath:String
    startTime:String
    endTime:String
```

响应只包含录像元数据，不包含视频字节。录像文件创建、播放和存在性校验由媒体/回放模块负责。

## 7. 错误码

| 数值 | 名称 | 含义 |
|---:|---|---|
| `0` | `SUCCESS` | 成功 |
| `1001` | `INVALID_PARAMETER` | 参数为空、越界或时间范围错误 |
| `1002` | `INVALID_PACKET` | Payload 截断、尾随数据或字段格式错误 |
| `1003` | `UNSUPPORTED_VERSION` | 不支持的协议版本 |
| `1004` | `BODY_TOO_LARGE` | Payload 声明长度超过 1 MiB |
| `1005` | `UNKNOWN_MESSAGE` | 未定义的消息 Type |
| `2001` | `DATABASE_ERROR` | 数据库连接、查询或事务失败 |
| `2002` | `USER_ALREADY_EXISTS` | 用户名已注册 |
| `2003` | `USER_NOT_FOUND` | 用户不存在 |
| `2004` | `PASSWORD_ERROR` | 密码验证失败 |
| `2005` | `UNAUTHORIZED` | token 无效、过期或与 userId 不匹配 |
| `9000` | `INTERNAL_ERROR` | 密码派生、随机数等内部操作失败 |

## 8. TCP 拆包规则

- 缓冲区不足 12 字节：保留原数据并等待后续字节；
- Header 完整但 Payload 不完整：保留整个半包；
- 一次收到多个包：每次只消费一条完整消息，再循环解析剩余数据；
- Length 超过 1 MiB：立即拒绝并清理当前坏包缓冲，防止超大长度攻击；
- Type 未知、Version 不支持或 Payload 有尾随字节：拒绝该消息，不进入业务层。

## 9. 认证安全

- `users` 只保存 PBKDF2-HMAC-SHA256 密码摘要和每用户随机 salt；
- PBKDF2 当前迭代次数为 100000，摘要长度 32 字节；
- token 使用 32 字节安全随机数生成，明文只返回给客户端进程内存；
- `user_sessions` 只保存 token 的 SHA-512 摘要、过期时间和撤销时间；
- 协议日志不得输出 password、salt、明文 token、摄像头账号或真实流地址。

## 10. 代码位置

- 外层 TLV：`common/include/protocol/Protocol.h`、`common/src/Protocol.cc`；
- 认证：`AuthProtocol.h/.cc`；
- 设备：`DeviceProtocol.h/.cc`；
- 录像：`RecordProtocol.h/.cc`；
- Qt 适配：`client/qt/SmartHomeClient/protocol/ClientProtocol.h/.cpp`；
- 协议测试：`common/tests` 和 `client/qt/SmartHomeClient/tests/ClientProtocolTest.cpp`。
