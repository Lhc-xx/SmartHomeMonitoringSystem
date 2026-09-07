#ifndef RESOURCE_HANDLER_H
#define RESOURCE_HANDLER_H

#include "DeviceService.h"
#include "RecordService.h"
#include "protocol/Protocol.h"

#include <cstdint>
#include <string>

namespace smart_home {

/* ResourceHandler 负责设备列表和录像查询的会话校验、业务调用及响应封装。 */
class ResourceHandler {
public:
    /* 所有资源查询共用同一数据库连接和会话数据。 */
    explicit ResourceHandler(MySQLClient &mysql);

    /* 处理 DEVICE_LIST_REQUEST 或 RECORD_QUERY_REQUEST。 */
    TlvMessage handle(const TlvMessage &request);

private:
    /* 仅通过 token 摘要查询仍在有效期内的会话。 */
    bool hasActiveSession(uint64_t userId, const std::string &token);
    TlvMessage handleDeviceList(const TlvMessage &request);
    TlvMessage handleRecordQuery(const TlvMessage &request);
    static bool isValidDateTime(const std::string &value);

    MySQLClient &_mysql;
    DeviceService _deviceService;
    RecordService _recordService;
};

} // namespace smart_home

#endif // RESOURCE_HANDLER_H
