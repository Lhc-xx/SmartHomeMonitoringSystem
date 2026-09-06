#include "AuthHandler.h"

#include "protocol/AuthProtocol.h"
#include "protocol/ErrorCode.h"
#include "protocol/MessageType.h"

#include <string>

namespace smart_home {

namespace {

/* 将注册错误转换为可直接展示的消息；绝不包含密码、salt 或 token。 */
std::string registerMessageForCode(ErrorCode code) {
    switch (code) {
    case ErrorCode::SUCCESS:
        return "注册成功";
    case ErrorCode::INVALID_PARAMETER:
        return "用户名或密码格式不正确";
    case ErrorCode::USER_ALREADY_EXISTS:
        return "用户名已存在";
    case ErrorCode::INVALID_PACKET:
        return "请求数据格式错误";
    case ErrorCode::DATABASE_ERROR:
        return "服务暂时不可用";
    default:
        return "注册失败，请稍后重试";
    }
}

} // namespace

AuthHandler::AuthHandler(UserService &service)
    : _service(service) {}

TlvMessage AuthHandler::handle(const TlvMessage &request) {
    /* 认证消息按类型分流，确保客户端不会把登录响应误当成注册响应。 */
    if (request.type == static_cast<uint16_t>(MessageType::REGISTER_REQUEST)) {
        return handleRegister(request);
    }
    if (request.type == static_cast<uint16_t>(MessageType::LOGIN_REQUEST)) {
        return handleLogin(request);
    }

    /* 未知认证类型返回统一错误，同时保留 requestId 便于客户端关联请求。 */
    TlvMessage response;
    response.type = static_cast<uint16_t>(MessageType::REGISTER_RESPONSE);
    response.requestId = request.requestId;
    response.value = AuthProtocol::encodeRegisterResponse(
        ErrorCode::UNKNOWN_MESSAGE, registerMessageForCode(ErrorCode::UNKNOWN_MESSAGE));
    return response;
}

TlvMessage AuthHandler::handleRegister(const TlvMessage &request) {
    std::string username;
    std::string password;
    ErrorCode code = ErrorCode::INVALID_PACKET;

    /* value 解析失败时不进入数据库，防止损坏载荷触发无意义的业务操作。 */
    if (AuthProtocol::decodeRegisterRequest(request.value, username, password)) {
        code = _service.registerUser(username, password);
    }

    TlvMessage response;
    response.type = static_cast<uint16_t>(MessageType::REGISTER_RESPONSE);
    response.requestId = request.requestId;
    response.value = AuthProtocol::encodeRegisterResponse(code,
                                                           registerMessageForCode(code));
    return response;
}

TlvMessage AuthHandler::handleLogin(const TlvMessage &request) {
    std::string username;
    std::string password;
    LoginResult result = {0U, std::string(), ErrorCode::INVALID_PACKET};

    /* 登录凭据只在内存中解码并交给 UserService，响应失败时不会携带 token。 */
    if (AuthProtocol::decodeLoginRequest(request.value, username, password)) {
        result = _service.loginUser(username, password);
    }

    TlvMessage response;
    response.type = static_cast<uint16_t>(MessageType::LOGIN_RESPONSE);
    response.requestId = request.requestId;
    AuthProtocol::encodeLoginResponse(result.userId, result.token, result.code,
                                      response.value);
    return response;
}

} // namespace smart_home
