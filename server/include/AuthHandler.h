#ifndef AUTH_HANDLER_H
#define AUTH_HANDLER_H

#include "UserService.h"
#include "protocol/Protocol.h"

namespace smart_home {

/*
 * AuthHandler 负责把认证 TLV 请求转换为 UserService 调用，再把业务结果编码为响应。
 * 该类不拥有数据库连接，也不参与 A 成员的连接生命周期和消息分发实现。
 */
class AuthHandler {
public:
    /* 注入已连接的业务服务，便于启动层统一管理资源生命周期。 */
    explicit AuthHandler(UserService &service);

    /* 根据消息类型处理注册或登录，始终保留原 requestId。 */
    TlvMessage handle(const TlvMessage &request);

private:
    /* 分别处理两种认证请求，避免响应类型混用。 */
    TlvMessage handleRegister(const TlvMessage &request);
    TlvMessage handleLogin(const TlvMessage &request);

    UserService &_service;
};

} // namespace smart_home

#endif // AUTH_HANDLER_H
