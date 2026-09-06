#ifndef DEVICE_SERVICE_H
#define DEVICE_SERVICE_H

#include "MySQLClient.h"
#include "protocol/DeviceProtocol.h"

#include <cstdint>
#include <vector>

namespace smart_home {

/* DeviceService 只按用户维度读取设备元数据，不处理网络协议和会话生命周期。 */
class DeviceService {
public:
    /* 复用启动层创建的 MySQLClient，避免业务层各自建立连接。 */
    explicit DeviceService(MySQLClient &mysql);

    /* 查询成功（包括空列表）返回 true，数据库错误返回 false。 */
    bool listByUser(uint64_t userId, std::vector<DeviceInfo> &devices);

private:
    MySQLClient &_mysql;
};

} // namespace smart_home

#endif // DEVICE_SERVICE_H
