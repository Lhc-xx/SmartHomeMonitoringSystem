#include "DeviceService.h"

#include <mysql/mysql.h>

#include <cstdlib>
#include <string>

namespace smart_home {

DeviceService::DeviceService(MySQLClient &mysql)
    : _mysql(mysql) {}

bool DeviceService::listByUser(uint64_t userId,
                               std::vector<DeviceInfo> &devices) {
    /* 输出先清空，保证数据库失败或重复调用时不会残留旧数据。 */
    devices.clear();
    MYSQL_RES *result = _mysql.query(
        "SELECT id,device_name,device_type,status FROM devices WHERE user_id=" +
        std::to_string(userId) + " ORDER BY id");
    if (result == nullptr) {
        return false;
    }

    MYSQL_ROW row = nullptr;
    while ((row = mysql_fetch_row(result)) != nullptr) {
        /* schema 字段均为 NOT NULL；额外检查可避免异常历史数据导致崩溃。 */
        if (row[0] == nullptr || row[1] == nullptr || row[2] == nullptr ||
            row[3] == nullptr) {
            mysql_free_result(result);
            devices.clear();
            return false;
        }
        DeviceInfo item;
        item.id = static_cast<uint64_t>(std::strtoull(row[0], nullptr, 10));
        item.deviceName = row[1];
        item.deviceType = row[2];
        item.status = row[3];
        devices.push_back(item);
    }
    mysql_free_result(result);
    return true;
}

} // namespace smart_home
