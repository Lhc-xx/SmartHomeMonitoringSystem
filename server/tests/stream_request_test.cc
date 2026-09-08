#include "stream_request.h"

#include <cstdint>
#include <iostream>
#include <string>
#include <vector>

namespace {

bool expect(bool condition, const char *message)
{
    if (!condition) {
        std::cerr << "[FAIL] " << message << "\n";
        return false;
    }
    return true;
}

bool testStreamStartStrictLength()
{
    bool ok = true;
    std::string url;
    const std::string text = "rtsp://camera/live";
    std::vector<uint8_t> value;
    value.push_back(static_cast<uint8_t>((text.size() >> 8) & 0xffU));
    value.push_back(static_cast<uint8_t>(text.size() & 0xffU));
    value.insert(value.end(), text.begin(), text.end());

    ok &= expect(smart_home::parseStreamStartValue(value, url),
                 "合法 stream URL 应该解析成功");
    ok &= expect(url == text, "解析结果必须保持原始 URL");

    std::vector<uint8_t> truncated = value;
    truncated.pop_back();
    ok &= expect(!smart_home::parseStreamStartValue(truncated, url),
                 "截断 stream URL 必须拒绝");

    std::vector<uint8_t> trailing = value;
    trailing.push_back(0);
    ok &= expect(!smart_home::parseStreamStartValue(trailing, url),
                 "带尾随垃圾字节的 stream URL 必须拒绝");
    return ok;
}

bool testRecordStartFixedWidth()
{
    bool ok = true;
    uint64_t deviceId = 0;
    const std::vector<uint8_t> value = {0, 0, 0, 0, 0, 0, 0, 42};
    ok &= expect(smart_home::parseRecordStartValue(value, deviceId),
                 "八字节 deviceId 应该解析成功");
    ok &= expect(deviceId == 42, "deviceId 必须按大端序解析");

    std::vector<uint8_t> shortValue = value;
    shortValue.pop_back();
    ok &= expect(!smart_home::parseRecordStartValue(shortValue, deviceId),
                 "短 deviceId 必须拒绝");

    std::vector<uint8_t> extraValue = value;
    extraValue.push_back(0);
    ok &= expect(!smart_home::parseRecordStartValue(extraValue, deviceId),
                 "带尾随字段的 deviceId 必须拒绝");
    return ok;
}

bool testEmptyControlValue()
{
    bool ok = true;
    ok &= expect(smart_home::isEmptyControlValue(std::vector<uint8_t>()),
                 "停流/停录的空 value 应该通过");
    ok &= expect(!smart_home::isEmptyControlValue(std::vector<uint8_t>(1, 0)),
                 "停流/停录携带字段必须拒绝");
    return ok;
}

} // namespace

int main()
{
    const bool ok = testStreamStartStrictLength()
        && testRecordStartFixedWidth()
        && testEmptyControlValue();
    std::cout << (ok ? "stream_request_test passed\n" : "stream_request_test failed\n");
    return ok ? 0 : 1;
}
