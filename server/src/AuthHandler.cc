#include "AuthHandler.h"

#include "protocol/AuthProtocol.h"
#include "protocol/ErrorCode.h"
#include "protocol/MessageType.h"

#include <string>

namespace smart_home {

AuthHandler::AuthHandler(UserService &service)
    : _service(service) {}

TlvMessage AuthHandler::handle(const TlvMessage &request) {
    /*
     * 当前认证处理层只支持注册消息。后续登录等认证消息可以在这里
     * 增加分支，但未知类型必须明确返回协议层定义的错误码。
     */
    if(request.type ==
       static_cast<uint16_t>(MessageType::REGISTER_REQUEST)) {
        return handleRegister(request);
    }

    /*
     * 未支持的认证消息仍使用 REGISTER_RESPONSE 返回 ErrorCode，
     * 让调用方能够沿用同一条注册响应解码路径获取 UNKNOWN_MESSAGE。
     */
    TlvMessage response;
    response.type =
        static_cast<uint16_t>(MessageType::REGISTER_RESPONSE);

    /*
     * TlvMessage 默认构造时已经使用 PROTOCOL_VERSION，因此这里不设置
     * version，严格保持协议结构体的默认版本行为。
     */
    response.requestId = request.requestId;
    response.value = AuthProtocol::encodeRegisterResponse(
        ErrorCode::UNKNOWN_MESSAGE
    );

    return response;
}

TlvMessage AuthHandler::handleRegister(const TlvMessage &request) {
    std::string username;
    std::string password;

    /*
     * value 的字段布局只由 AuthProtocol 解释。解析失败说明载荷损坏或
     * 格式不符合认证协议，此时禁止调用 UserService。
     */
    ErrorCode code = ErrorCode::INVALID_PACKET;
    if(AuthProtocol::decodeRegisterRequest(
            request.value,
            username,
            password
        )) {
        /*
         * 解码成功后，将用户输入交给既有业务层。参数校验、查重、
         * 密码哈希和数据库写入仍全部由 UserService 负责。
         */
        code = _service.registerUser(username, password);
    }

    TlvMessage response;
    response.type =
        static_cast<uint16_t>(MessageType::REGISTER_RESPONSE);

    /* 使用默认协议版本，并完整关联请求和响应的 requestId。 */
    response.requestId = request.requestId;
    response.value = AuthProtocol::encodeRegisterResponse(code);

    return response;
}

} // namespace smart_home