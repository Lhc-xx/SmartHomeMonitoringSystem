#ifndef PTZ_HANDLER_H
#define PTZ_HANDLER_H

#include <string>
#include <set>

#include "PtzHttpClient.h"
#include "protocol/Protocol.h"

namespace smart_home {

/*
 * PtzHandler：把 PTZ_CONTROL_REQUEST 转发到摄像头网页 API，并返回 PTZ_CONTROL_RESPONSE。
 *
 * 请求 payload = cameraUrl(String) + direction(String) + move(String)。
 *   - direction 取值 up/down/left/right/up-left/... / stop；
 *   - move 取值 start/stop。
 * 响应 payload = errorCode(int32)。
 *
 * cameraUrl 来自客户端，不能直接交给 libcurl；本类先校验 http(s) 协议、
 * IPv4 字面量、端口和动作白名单，再与 server.conf 的允许主机集合比对，
 * 这样服务端不会被利用为访问任意内网地址的代理。
 */
class PtzHandler {
public:
    /* allowedHostsCsv 为空时拒绝所有目标，必须显式配置摄像头 IPv4 白名单。 */
    explicit PtzHandler(const std::string &secret,
                        const std::string &allowedHostsCsv = std::string());
    TlvMessage handle(const TlvMessage &msg);

private:
    bool isAllowedRequest(const std::string &cameraUrl,
                          const std::string &direction,
                          const std::string &move) const;

    PtzHttpClient _client;
    std::set<std::string> _allowedHosts;
};

} // namespace smart_home

#endif // PTZ_HANDLER_H
