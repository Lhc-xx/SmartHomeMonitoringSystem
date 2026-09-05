#ifndef AUTH_HANDLER_H
#define AUTH_HANDLER_H

#include "protocol/Protocol.h"
#include "UserService.h"

namespace smart_home {

/*
 * AuthHandler
 *
 * Server 认证消息的业务处理层。
 *
 * 它只负责连接“TLV 消息边界”与“UserService 业务层”：
 *
 * REGISTER_REQUEST TlvMessage
 *     -> AuthProtocol 解码 value
 *     -> UserService::registerUser()
 *     -> AuthProtocol 编码 ErrorCode
 *     -> REGISTER_RESPONSE TlvMessage
 *
 * AuthHandler 不拥有 UserService，也不直接访问数据库；这样数据库和
 * 用户业务规则仍集中在既有 UserService 中。
 */
class AuthHandler {
public:
    /*
     * 注入已经创建好的用户业务服务。
     *
     * 引用明确表达 AuthHandler 只借用服务，生命周期由服务器启动层管理。
     */
    explicit AuthHandler(UserService &service);

    /*
     * 根据请求消息类型处理认证业务，并始终构造一个对应的响应消息。
     */
    TlvMessage handle(const TlvMessage &request);

private:
    /*
     * 处理 REGISTER_REQUEST：解析认证 value、调用注册业务，并将结果
     * 编码为 REGISTER_RESPONSE。
     */
    TlvMessage handleRegister(const TlvMessage &request);

    /* AuthHandler 不拥有该对象，只负责通过它执行用户注册业务。 */
    UserService &_service;
};

} // namespace smart_home

#endif // AUTH_HANDLER_H