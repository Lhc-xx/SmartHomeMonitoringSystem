#ifndef PTZ_HANDLER_H
#define PTZ_HANDLER_H

#include <string>

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
 */
class PtzHandler {
public:
    explicit PtzHandler(const std::string &secret);
    TlvMessage handle(const TlvMessage &msg);

private:
    PtzHttpClient _client;
};

} // namespace smart_home

#endif // PTZ_HANDLER_H
