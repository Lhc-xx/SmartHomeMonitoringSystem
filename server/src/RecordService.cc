#include "RecordService.h"

#include <mysql/mysql.h>

#include <cstdlib>

namespace smart_home {

RecordService::RecordService(MySQLClient &mysql)
    : _mysql(mysql) {}

bool RecordService::queryByDevice(uint64_t userId,
                                  uint64_t deviceId,
                                  const std::string &startTime,
                                  const std::string &endTime,
                                  std::vector<RecordInfo> &records) {
    /* 归属条件直接放入 JOIN/WHERE，避免先查设备再查录像的竞态越权窗口。 */
    records.clear();
    const std::string escapedStart = _mysql.escape(startTime);
    const std::string escapedEnd = _mysql.escape(endTime);
    MYSQL_RES *result = _mysql.query(
        "SELECT r.id,r.device_id,r.file_path,"
        "DATE_FORMAT(r.start_time,'%Y-%m-%d %H:%i:%s'),"
        "DATE_FORMAT(r.end_time,'%Y-%m-%d %H:%i:%s') "
        "FROM records r INNER JOIN devices d ON r.device_id=d.id "
        "WHERE r.device_id=" + std::to_string(deviceId) +
        " AND d.user_id=" + std::to_string(userId) +
        " AND r.start_time>='" + escapedStart + "'" +
        " AND r.end_time<='" + escapedEnd + "' ORDER BY r.start_time,r.id");
    if (result == nullptr) {
        return false;
    }

    MYSQL_ROW row = nullptr;
    while ((row = mysql_fetch_row(result)) != nullptr) {
        /* 查询列均应非空，异常数据按数据库错误处理而不是返回半条记录。 */
        if (row[0] == nullptr || row[1] == nullptr || row[2] == nullptr ||
            row[3] == nullptr || row[4] == nullptr) {
            mysql_free_result(result);
            records.clear();
            return false;
        }
        RecordInfo item;
        item.id = static_cast<uint64_t>(std::strtoull(row[0], nullptr, 10));
        item.deviceId = static_cast<uint64_t>(std::strtoull(row[1], nullptr, 10));
        item.filePath = row[2];
        item.startTime = row[3];
        item.endTime = row[4];
        records.push_back(item);
    }
    mysql_free_result(result);
    return true;
}

bool RecordService::addRecord(uint64_t deviceId,
                              const std::string &filePath,
                              const std::string &startTime,
                              const std::string &endTime) {
    /* file_path/时间都是录像器产生的受控值，仍统一走 escape 防注入。 */
    const std::string sql =
        "INSERT INTO records (device_id,file_path,start_time,end_time) VALUES (" +
        std::to_string(deviceId) + ",'" + _mysql.escape(filePath) + "','" +
        _mysql.escape(startTime) + "','" + _mysql.escape(endTime) + "')";
    return _mysql.execute(sql);
}

} // namespace smart_home
