#ifndef AUTH_PROTOCOL_H
#define AUTH_PROTOCOL_H

#include "protocol/ErrorCode.h"
#include <cstdint>
#include <string>
#include <vector>

/*
 * AuthProtocol
 *
 * 用户认证业务协议。
 *
 * 注意：
 *
 * TlvProtocol负责最外层：
 *
 * type
 * version
 * length
 * requestId
 * value
 *
 *
 * AuthProtocol负责：
 *
 * value内部究竟怎么组织。
 *
 *
 * 当前Day2先实现：
 *
 * 1. REGISTER_REQUEST
 * 2. REGISTER_RESPONSE
 *
 *
 * 后面Day3登录可以继续在这个类中增加：
 *
 * encodeLoginRequest()
 * decodeLoginRequest()
 * encodeLoginResponse()
 * decodeLoginResponse()
 */

 class AuthProtocol{
    public:
    static bool encodeRegisterRequest(
        const std::string &username,
        const std::string &password,
        std::vector<uint8_t> &value
    );

    //注册请求解码
    //对register_request 当中的value 进行解析
    //成功
    //username / password 被填充
    //返回true
    static bool decodeRegisterRequest(
        const std::vector<uint8_t> &value,
        std::string &username,
        std::string &password
    );

    //注册响应编码
    //REGISTER_RESPONSE value
    static std::vector<uint8_t> encodeRegisterResponse(ErrorCode code);

    //注册响应解码
    static bool decodeRegisterResponse(const std::vector<uint8_t> &value,ErrorCode &code);
 };

#endif