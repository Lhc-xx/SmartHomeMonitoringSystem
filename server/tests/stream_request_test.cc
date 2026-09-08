#include "stream_request.h"

#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

namespace {

bool expect(bool condition, const char *message)
{
    if (!condition) {
        std::fprintf(stderr, "FAIL: %s\n", message);
        return false;
    }
    std::printf("PASS: %s\n", message);
    return true;
}

} // namespace

int main()
{
    bool ok = true;

    /* 空 URL 是协议允许的 Mock 流请求，仍然必须带完整的 uint16 长度字段。 */
    std::string url;
    ok = expect(smart_home::parseStreamStartValue({0, 0}, url) && url.empty(),
                "empty stream URL is valid") && ok;

    const std::vector<uint8_t> mockValue = {0, 9, 'm', 'o', 'c', 'k', ':', '/', '/', 't', '1'};
    ok = expect(smart_home::parseStreamStartValue(mockValue, url)
                    && url == "mock://t1",
                "complete stream URL is decoded") && ok;

    /* 长度字段与实际 value 不一致时，不能静默回退为 Mock。 */
    ok = expect(!smart_home::parseStreamStartValue({0}, url),
                "truncated stream length is rejected") && ok;
    ok = expect(!smart_home::parseStreamStartValue({0, 4, 'm', 'o'}, url),
                "short stream payload is rejected") && ok;
    ok = expect(!smart_home::parseStreamStartValue(
                    {0, 1, 'x', 't', 'a', 'i', 'l'}, url),
                "trailing stream bytes are rejected") && ok;

    /* 录像开始请求必须恰好携带一个大端 uint64 deviceId。 */
    uint64_t deviceId = 0;
    const std::vector<uint8_t> idValue = {0, 0, 0, 0, 0, 0, 0, 7};
    ok = expect(smart_home::parseRecordStartValue(idValue, deviceId) && deviceId == 7,
                "record device id uses big endian") && ok;
    ok = expect(!smart_home::parseRecordStartValue({0, 0, 0, 7}, deviceId),
                "short record device id is rejected") && ok;
    ok = expect(!smart_home::parseRecordStartValue({0, 0, 0, 0, 0, 0, 0, 7, 1}, deviceId),
                "trailing record bytes are rejected") && ok;

    ok = expect(smart_home::isEmptyControlValue(std::vector<uint8_t>()),
                "empty stop request is valid") && ok;
    ok = expect(!smart_home::isEmptyControlValue({0}),
                "non-empty stop request is rejected") && ok;

    return ok ? 0 : 1;
}
