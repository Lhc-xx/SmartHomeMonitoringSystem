#include "stream_request.h"

namespace smart_home {

bool parseStreamStartValue(const std::vector<uint8_t> &value, std::string &streamUrl)
{
    streamUrl.clear();
    /* 至少要有 uint16 长度字段；不足时不能把请求降级为 Mock。 */
    if (value.size() < 2) {
        return false;
    }

    const uint16_t length = static_cast<uint16_t>(
        (static_cast<uint16_t>(value[0]) << 8) | static_cast<uint16_t>(value[1]));
    const std::size_t expectedSize = static_cast<std::size_t>(2) + length;
    /* 必须精确匹配，拒绝截断和长度字段后附带的垃圾字节。 */
    if (value.size() != expectedSize) {
        return false;
    }

    streamUrl.assign(reinterpret_cast<const char *>(value.data() + 2), length);
    return true;
}

bool parseRecordStartValue(const std::vector<uint8_t> &value, uint64_t &deviceId)
{
    deviceId = 0;
    /* deviceId 是固定宽度字段，短包和尾随字段都属于非法请求。 */
    if (value.size() != 8) {
        return false;
    }
    for (std::size_t index = 0; index < value.size(); ++index) {
        deviceId = (deviceId << 8) | static_cast<uint64_t>(value[index]);
    }
    return true;
}

bool isEmptyControlValue(const std::vector<uint8_t> &value)
{
    return value.empty();
}

} // namespace smart_home
