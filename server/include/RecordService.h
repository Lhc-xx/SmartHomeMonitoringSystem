#ifndef RECORD_SERVICE_H
#define RECORD_SERVICE_H

#include "MySQLClient.h"
#include "protocol/RecordProtocol.h"

#include <cstdint>
#include <string>
#include <vector>

namespace smart_home {

/* RecordService 提供带设备归属校验的录像元数据查询，不涉及 FFmpeg 播放。 */
class RecordService {
public:
    /* MySQLClient 的生命周期由启动层管理，服务类仅保存引用。 */
    explicit RecordService(MySQLClient &mysql);

    /* SQL 同时校验 userId 与 deviceId，避免跨用户枚举录像。 */
    bool queryByDevice(uint64_t userId,
                       uint64_t deviceId,
                       const std::string &startTime,
                       const std::string &endTime,
                       std::vector<RecordInfo> &records);

    /* 录像停止时写入一条元数据索引；时间格式 yyyy-MM-dd HH:mm:ss。 */
    bool addRecord(uint64_t deviceId,
                   const std::string &filePath,
                   const std::string &startTime,
                   const std::string &endTime);

private:
    MySQLClient &_mysql;
};

} // namespace smart_home

#endif // RECORD_SERVICE_H
