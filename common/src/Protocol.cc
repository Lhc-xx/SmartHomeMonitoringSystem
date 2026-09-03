#include "protocol/Protocol.h"

#include <arpa/inet.h>
#include <cstdint>
#include <cstring>
#include <netinet/in.h>
#include <vector>

std::vector<uint8_t> TlvProtocol::encode(const TlvMessage &message) {

  uint32_t bodyLength = message.value.size();
  if (bodyLength > MAX_TLV_BODY_SIZE) {
    return {};
  }
  uint32_t totalSize = TLV_HEADER_SIZE + bodyLength;

  //创建发送缓冲区
  std::vector<uint8_t> buffer;
  buffer.resize(totalSize);

  //当前写入的位置
  uint8_t *ptr = buffer.data();

  uint16_t type = htons(message.type);

  memcpy(ptr, &type, sizeof(type));

  ptr += sizeof(type);

  //写入Version
  uint16_t version = htons(message.version);

  memcpy(ptr, &version, sizeof(version));
  ptr += sizeof(version);

  //写入Length
  uint32_t length = htonl(bodyLength);

  memcpy(ptr, &length, sizeof(length));

  ptr += sizeof(length);

  /*
   * 写入RequestId
   */
  uint32_t requestId = htonl(message.requestId);

  memcpy(ptr, &requestId, sizeof(requestId));

  ptr += sizeof(requestId);

  /*
   * 最后写Value
   */
  if (bodyLength > 0) {
    memcpy(ptr, message.value.data(), bodyLength);
  }
  return buffer;
}

bool TlvProtocol::tryDecode(
        std::vector<uint8_t> &buffer,
        TlvMessage &message)
{


    /*
     * 第一关：
     *
     * Header够不够
     */
    if(buffer.size() < TLV_HEADER_SIZE)
    {
        return false;
    }



    const uint8_t *ptr =
        buffer.data();



    uint16_t type;


    memcpy(&type,
           ptr,
           sizeof(type));


    ptr += sizeof(type);



    uint16_t version;


    memcpy(&version,
           ptr,
           sizeof(version));


    ptr += sizeof(version);



    uint32_t length;


    memcpy(&length,
           ptr,
           sizeof(length));



    length = ntohl(length);



    /*
     * 防止4GB攻击
     */
    if(length > MAX_TLV_BODY_SIZE)
    {
        buffer.clear();

        return false;
    }



    /*
     * 判断完整包
     */
    if(buffer.size()
       <
       TLV_HEADER_SIZE + length)
    {
        return false;
    }



    ptr += sizeof(length);



    uint32_t requestId;


    memcpy(&requestId,
           ptr,
           sizeof(requestId));



    requestId =
        ntohl(requestId);



    ptr += sizeof(requestId);



    /*
     * 填充message
     */

    message.type =
        ntohs(type);


    message.version =
        ntohs(version);


    message.requestId =
        requestId;



    message.value.assign(
        ptr,
        ptr + length
    );



    /*
     * 删除已经消费的数据
     */
    buffer.erase(
        buffer.begin(),
        buffer.begin()
        +
        TLV_HEADER_SIZE
        +
        length
    );

    return true;
}