#ifndef RECORD_PROTOCOL_H
#define RECORD_PROTOCOL_H

#include "protocol/ErrorCode.h"

#include <cstdint>
#include <string>
#include <vector>

/* 录像项保存主键、所属设备和三个 UTF-8 文本字段，字段顺序由 RecordProtocol 固定。 */
struct RecordInfo {
  uint64_t id = 0U;
  uint64_t deviceId = 0U;
  std::string filePath;
  std::string startTime;
  std::string endTime;
};

/* 录像查询业务 value 的编解码器；不包含外层 TLV 头。 */
class RecordProtocol {
 public:
  /* 请求：userId + tokenLen/token + deviceId + startTimeLen/startTime + endTimeLen/endTime。 */
  static bool encodeRecordQueryRequest(uint64_t userId, const std::string &token,
                                       uint64_t deviceId, const std::string &startTime,
                                       const std::string &endTime,
                                       std::vector<uint8_t> &value);
  static bool decodeRecordQueryRequest(const std::vector<uint8_t> &value,
                                       uint64_t &userId, std::string &token,
                                       uint64_t &deviceId, std::string &startTime,
                                       std::string &endTime);

  /* 响应：errorCode + count + 每项 id/deviceId 与 filePath/startTime/endTime 三段字符串。 */
  static bool encodeRecordQueryResponse(ErrorCode errorCode,
                                        const std::vector<RecordInfo> &records,
                                        std::vector<uint8_t> &value);
  static bool decodeRecordQueryResponse(const std::vector<uint8_t> &value,
                                        ErrorCode &errorCode,
                                        std::vector<RecordInfo> &records);
};

#endif
