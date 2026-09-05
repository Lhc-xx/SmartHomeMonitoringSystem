#include "protocol/AuthProtocol.h"

#include <cstdint>
#include <limits>
#include <string>
#include <vector>

namespace {
void appendUint16BE(std::vector<uint8_t> &buffer, uint16_t value) {
  /*
   * ============================================================
   * 写入一个uint16_t网络大端整数
   * ============================================================
   *
   * 例如：
   *
   * value = 0x1234
   *
   * 写入：
   *
   * 12 34
   *
   *
   * 为什么不用htons()？
   *
   * 因为common最终还要给Windows Qt使用。
   *
   * Linux有：
   *
   * <arpa/inet.h>
   *
   * Windows环境并不直接提供这个头文件。
   *
   * 所以这里手动写大端字节，
   * 让AuthProtocol本身跨平台。
   */
  /*
   * 高8位。
   */
  buffer.push_back(static_cast<uint8_t>((value >> 8) & 0xFF));

  /*
   * 低8位。
   */
  buffer.push_back(static_cast<uint8_t>(value & 0xFF));
}

/*
 * ============================================================
 * 从buffer读取一个uint16_t网络大端整数
 * ============================================================
 *
 * 例如buffer里面：
 *
 * 00 03
 *
 * 解析以后：
 *
 * value = 3
 *
 * position会向后移动2个字节。
 */
bool readUint16BE(
    const std::vector<uint8_t> &buffer,
    std::size_t &position,
    uint16_t &value
)
{
    /*
     * uint16_t需要2个字节。
     *
     * 如果剩余数据不足2字节，
     * 说明数据包被截断。
     */
    if(position + 2 > buffer.size())
    {
        return false;
    }


    /*
     * 网络协议采用大端：
     *
     * buffer[position]     高8位
     * buffer[position + 1] 低8位
     *
     * 例如：
     *
     * 00 03
     *
     *      ↓
     *
     * 0x0003
     *
     *      ↓
     *
     * 3
     */
    value =
        static_cast<uint16_t>(
            static_cast<uint16_t>(
                buffer[position]
            ) << 8
        )
        |
        static_cast<uint16_t>(
            buffer[position + 1]
        );


    /*
     * 两个字节已经被消费，
     * 读取位置向后移动。
     */
    position += 2;


    return true;
}

/*
 * ============================================================
 * 写入一个uint32_t网络大端整数
 * ============================================================
 *
 * 例如：
 *
 * value = 0x12345678
 *
 * 写入buffer：
 *
 * 12 34 56 78
 *
 *
 * 当前主要用于：
 *
 * ErrorCode
 *
 * 因为ErrorCode底层类型是int32_t，
 * 注册响应需要使用4个字节传输。
 */
void appendUint32BE(
    std::vector<uint8_t> &buffer,
    uint32_t value
)
{
    /*
     * 最高8位
     */
    buffer.push_back(
        static_cast<uint8_t>(
            (value >> 24) & 0xFF
        )
    );


    /*
     * 第2个8位
     */
    buffer.push_back(
        static_cast<uint8_t>(
            (value >> 16) & 0xFF
        )
    );


    /*
     * 第3个8位
     */
    buffer.push_back(
        static_cast<uint8_t>(
            (value >> 8) & 0xFF
        )
    );


    /*
     * 最低8位
     */
    buffer.push_back(
        static_cast<uint8_t>(
            value & 0xFF
        )
    );
}


/*
 * ============================================================
 * 读取uint32_t网络大端整数
 * ============================================================
 */
bool readUint32BE(const std::vector<uint8_t> &buffer, std::size_t &position,
                  uint32_t &value) {
  if (position + 4 > buffer.size()) {
    return false;
  }

  value = (static_cast<uint32_t>(buffer[position]) << 24) |
          (static_cast<uint32_t>(buffer[position + 1]) << 16) |
          (static_cast<uint32_t>(buffer[position + 2]) << 8) |
          static_cast<uint32_t>(buffer[position + 3]);

  position += 4;

  return true;
}

} // anonymous namespace

/*
 * ============================================================
 * 注册请求编码
 * ============================================================
 */
bool AuthProtocol::encodeRegisterRequest(const std::string &username,
                                         const std::string &password,
                                         std::vector<uint8_t> &value) {
  /*
   * usernameLength/passwordLength
   * 使用uint16_t。
   *
   * 所以单字段不能超过65535 Byte。
   */
  if (username.size() > std::numeric_limits<uint16_t>::max()) {
    return false;
  }

  if (password.size() > std::numeric_limits<uint16_t>::max()) {
    return false;
  }

  /*
   * 每次编码前先清空旧数据。
   */
  value.clear();

  /*
   * 提前分配空间。
   *
   * 2
   * +
   * username
   * +
   * 2
   * +
   * password
   */
  value.reserve(2 + username.size() + 2 + password.size());

  // ========================================================
  // usernameLength
  // ========================================================

  appendUint16BE(value, static_cast<uint16_t>(username.size()));

  // ========================================================
  // username
  // ========================================================

  value.insert(value.end(), username.begin(), username.end());

  // ========================================================
  // passwordLength
  // ========================================================

  appendUint16BE(value, static_cast<uint16_t>(password.size()));

  // ========================================================
  // password
  // ========================================================

  value.insert(value.end(), password.begin(), password.end());

  return true;
}

/*
 * ============================================================
 * 注册请求解码
 * ============================================================
 */
bool AuthProtocol::decodeRegisterRequest(const std::vector<uint8_t> &value,
                                         std::string &username,
                                         std::string &password) {
  /*
   * 当前读取位置。
   */
  std::size_t position = 0;

  // ========================================================
  // 第1步：读usernameLength
  // ========================================================

  uint16_t usernameLength = 0;

  if (!readUint16BE(value, position, usernameLength)) {
    return false;
  }

  /*
   * usernameLength后面
   * 必须至少还有usernameLength个字节。
   */
  if (position + usernameLength > value.size()) {
    return false;
  }

  // ========================================================
  // 第2步：读取username
  // ========================================================

  username.assign(value.begin() + position,
                  value.begin() + position + usernameLength);

  position += usernameLength;

  // ========================================================
  // 第3步：读取passwordLength
  // ========================================================

  uint16_t passwordLength = 0;

  if (!readUint16BE(value, position, passwordLength)) {
    return false;
  }

  /*
   * 剩余数据必须刚好等于passwordLength。
   *
   * 注意这里使用：
   *
   * !=
   *
   * 而不是：
   *
   * <
   *
   *
   * 这样除了防止截断包，
   * 也可以拒绝末尾追加的垃圾数据。
   */
  if (value.size() - position != passwordLength) {
    return false;
  }

  // ========================================================
  // 第4步：读取password
  // ========================================================

  password.assign(value.begin() + position, value.end());

  position += passwordLength;

  /*
   * 理论上position必须刚好到达末尾。
   */
  return position == value.size();
}

/*
 * ============================================================
 * 注册响应编码
 * ============================================================
 */
std::vector<uint8_t> AuthProtocol::encodeRegisterResponse(ErrorCode code) {
  std::vector<uint8_t> value;

  /*
   * ErrorCode底层是int32_t，
   *
   * 当前错误码均为非负整数，
   * 转成uint32_t后按网络大端发送。
   */
  uint32_t rawCode = static_cast<uint32_t>(static_cast<int32_t>(code));

  /*
   * 固定4 Byte。
   */
  value.reserve(4);

  appendUint32BE(value, rawCode);

  return value;
}

/*
 * ============================================================
 * 注册响应解码
 * ============================================================
 */
bool AuthProtocol::decodeRegisterResponse(const std::vector<uint8_t> &value,
                                          ErrorCode &code) {
  /*
   * REGISTER_RESPONSE目前协议固定：
   *
   * ErrorCode = 4 Byte
   *
   * 多一个少一个都视为格式错误。
   */
  if (value.size() != 4) {
    return false;
  }

  std::size_t position = 0;

  uint32_t rawCode = 0;

  if (!readUint32BE(value, position, rawCode)) {
    return false;
  }

  code = static_cast<ErrorCode>(static_cast<int32_t>(rawCode));

  return position == value.size();
}
