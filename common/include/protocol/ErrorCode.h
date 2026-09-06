#ifndef ERROR_CODE_H
#define ERROR_CODE_H

#include <cstdint>

//整个项目统一的错误码
//Qt 和 Server 必须使用同一套的错误代码

enum class ErrorCode : int32_t{
    //成功
    SUCCESS = 0,

    //协议错误 1000+
    INVALID_PARAMETER   = 1001,

    INVALID_PACKET      = 1002,

    UNSUPPORTED_VERSION = 1003,

    BODY_TOO_LARGE      = 1004,

    UNKNOWN_MESSAGE     = 1005,

    //数据库 / 用户

    DATABASE_ERROR      = 2001,
    
    USER_ALREADY_EXISTS = 2002,

    USER_NOT_FOUND      = 2003,

    PASSWORD_ERROR      = 2004,

    UNAUTHORIZED        = 2005,

    //流媒体 / 录像 3000+
    STREAM_NOT_FOUND       = 3001,  // 无活动流会话
    RECORD_ALREADY_STARTED = 3002,  // 录像已开启
    RECORD_NOT_STARTED     = 3003,  // 录像未开启
    RECORD_OPEN_FAILED     = 3004,  // 录像文件创建失败

    //服务器内部错误
    INTERNAL_ERROR      = 9000
    
};

#endif