#include "protocol/RecordProtocol.h"

#include <cassert>
#include <cstdint>
#include <iostream>
#include <string>
#include <vector>

int main() {
  /* 查询请求使用 userId、token、deviceId 和两个时间字符串，并要求无尾随字节。 */
  std::vector<uint8_t> request;
  assert(RecordProtocol::encodeRecordQueryRequest(7, "token", 88,
                                                   "2026-09-01", "2026-09-02",
                                                   request));
  uint64_t userId = 0;
  uint64_t deviceId = 0;
  std::string token;
  std::string startTime;
  std::string endTime;
  assert(RecordProtocol::decodeRecordQueryRequest(request, userId, token,
                                                   deviceId, startTime, endTime));
  assert(userId == 7 && deviceId == 88 && token == "token");
  request.push_back(0x00);
  assert(!RecordProtocol::decodeRecordQueryRequest(request, userId, token,
                                                    deviceId, startTime, endTime));

  /* 空列表响应必须合法；包含一项时须完整还原两个 ID 和三段字符串。 */
  std::vector<RecordInfo> records;
  std::vector<uint8_t> response;
  assert(RecordProtocol::encodeRecordQueryResponse(ErrorCode::SUCCESS, records,
                                                    response));
  ErrorCode code = ErrorCode::INTERNAL_ERROR;
  assert(RecordProtocol::decodeRecordQueryResponse(response, code, records));
  assert(records.empty());

  RecordInfo record;
  record.id = 1;
  record.deviceId = 2;
  record.filePath = "D:/record.mp4";
  record.startTime = "2026-09-01T00:00:00Z";
  record.endTime = "2026-09-01T01:00:00Z";
  records.push_back(record);
  assert(RecordProtocol::encodeRecordQueryResponse(ErrorCode::SUCCESS, records,
                                                    response));
  records.clear();
  assert(RecordProtocol::decodeRecordQueryResponse(response, code, records));
  assert(records.size() == 1U && records[0].id == record.id &&
         records[0].deviceId == record.deviceId);
  assert(records[0].filePath == record.filePath);

  response.pop_back();
  assert(!RecordProtocol::decodeRecordQueryResponse(response, code, records));

  /* 所有长度字符串和响应项数均使用 uint16，超过上限必须被编码接口拒绝。 */
  std::string tooLong(65536U, 'x');
  assert(!RecordProtocol::encodeRecordQueryRequest(1, tooLong, 2, "start", "end",
                                                    request));
  records.assign(65536U, RecordInfo());
  assert(!RecordProtocol::encodeRecordQueryResponse(ErrorCode::SUCCESS, records,
                                                    response));
  std::cout << "[PASS] record_protocol_test" << std::endl;
  return 0;
}
