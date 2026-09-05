#ifndef MESSAGE_TYPE_H
#define MESSAGE_TYPE_H

#include <cstdint>

// TLV 消息类型 规定
// xxxx1  : Request
// xxxx2  : Response

// 0x1001 注册请求
// 0x1002 注册响应

enum class MessageType : uint16_t{
    //用户验证
    REGISTER_REQUEST = 0x1001,
    REGISTER_RESPONSE = 0x1002,

    LOGIN_REQUEST = 0x1101,
    LOGIN_RESPONSE = 0x1102,

    //设备
    DEVICE_LIST_REQUEST = 0x1201,
    DEVICE_LIST_RESPONSE = 0x1202,

    //录像
    RECORD_QUERY_REQUEST = 0x1301,
    RECORD_QUERY_RESPONSE = 0x1302
};

#endif