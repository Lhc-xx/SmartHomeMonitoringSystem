#ifndef CLIENTPROTOCOL_H
#define CLIENTPROTOCOL_H

#include <QByteArray>
#include <QList>
#include <QString>

#include "protocol/ErrorCode.h"

/*
 * ClientProtocol 只负责字节级 TLV 编解码：它不访问 Socket，也不保存登录状态。
 * 所有数值字段均使用网络大端序，以便与服务端和不同 CPU 架构保持一致。
 */
class ClientProtocol
{
public:
    /* 固定的协议版本和公共消息类型，避免 UI 与服务层散落魔法数字。 */
    enum MessageType {
        RegisterRequest = 0x1001, RegisterResponseType = 0x1002,
        LoginRequest = 0x1101, LoginResponseType = 0x1102,
        DeviceListRequest = 0x1201, DeviceListResponseType = 0x1202,
        RecordQueryRequest = 0x1301, RecordQueryResponseType = 0x1302,
        StreamStartRequest = 0x1401, StreamStartResponseType = 0x1402,
        StreamStopRequest = 0x1501, StreamStopResponseType = 0x1502,
        RecordStartRequest = 0x1601, RecordStartResponseType = 0x1602,
        RecordStopRequest = 0x1701, RecordStopResponseType = 0x1702,
        PtzControlRequest = 0x1801, PtzControlResponseType = 0x1802
    };

    /* 通用拆包状态：Incomplete 保留半包，Invalid 会安全清空无法恢复的缓冲。 */
    enum class PacketState { Incomplete, Decoded, Invalid };

    /* 一条完整 TLV 的已验证头字段和原始正文，供 UserService 按 requestId 分流。 */
    struct Packet {
        quint16 type;
        quint16 version;
        quint32 requestId;
        QByteArray body;
        /* 保留完整已验证包，让业务层可调用类型专属解码器而无需自行重组 TLV。 */
        QByteArray raw;
    };

    /* 注册、登录、设备和录像响应对应的业务数据对象，不含任何 Socket 或视频内容。 */
    struct RegisterResponse { quint32 requestId; ErrorCode errorCode; QString message; };
    struct LoginResponse { quint32 requestId; quint64 userId; QByteArray token; ErrorCode errorCode; };
    struct DeviceInfo { quint64 id; QString name; QString type; QString status; };
    struct RecordInfo { quint64 id; quint64 deviceId; QString filePath; QString startTime; QString endTime; };
    struct DeviceListResponse { quint32 requestId; ErrorCode errorCode; QList<DeviceInfo> devices; };
    struct RecordQueryResponse { quint32 requestId; ErrorCode errorCode; QList<RecordInfo> records; };

    /* 流媒体与录像控制的响应只携带 errorCode，四类复用同一结构。 */
    struct ControlResponse { quint32 requestId; ErrorCode errorCode; };

    /* 注册和登录请求的 value 均为 username/password 两个 uint16 length-string。 */
    static QByteArray encodeRegisterRequest(const QString &username, const QString &password,
                                            quint32 requestId = 0);
    static QByteArray encodeLoginRequest(const QString &username, const QString &password,
                                         quint32 requestId);

    /* 已认证请求携带 userId、token，再带各自所需的设备或时间筛选字段。 */
    static QByteArray encodeDeviceListRequest(quint64 userId, const QByteArray &token,
                                              quint32 requestId);
    static QByteArray encodeRecordQueryRequest(quint64 userId, const QByteArray &token,
                                               quint64 deviceId, const QString &startTime,
                                               const QString &endTime, quint32 requestId);

    /* 流媒体控制：推流请求携带 streamUrl（空串=Mock），停流无业务字段，录像开始携带 deviceId。 */
    static QByteArray encodeStreamStartRequest(const QString &streamUrl = QString(),
                                               quint32 requestId = 0);
    static QByteArray encodeStreamStopRequest(quint32 requestId = 0);
    static QByteArray encodeRecordStartRequest(quint64 deviceId, quint32 requestId = 0);
    static QByteArray encodeRecordStopRequest(quint32 requestId = 0);

    /* 云台控制：payload = cameraUrl(String) + direction(String) + move(String)。 */
    static QByteArray encodePtzControlRequest(const QString &cameraUrl,
                                              const QString &direction,
                                              const QString &move,
                                              quint32 requestId = 0);

    /* 从 TCP 缓冲取得一条完整 TLV；恶意超长 length 会安全失败而不会越界读取。 */
    static PacketState tryTakePacket(QByteArray &receiveBuffer, Packet &packet,
                                     ErrorCode &errorCode);

    /* 各响应解析函数会验证完整 TLV、版本、类型和正文所有长度字段。 */
    static bool decodeRegisterResponse(const QByteArray &packet, RegisterResponse &response);
    static bool decodeLoginResponse(const QByteArray &packet, LoginResponse &response);
    static bool decodeDeviceListResponse(const QByteArray &packet, DeviceListResponse &response);
    static bool decodeRecordQueryResponse(const QByteArray &packet, RecordQueryResponse &response);
    static bool decodeStreamStartResponse(const QByteArray &packet, ControlResponse &response);
    static bool decodeStreamStopResponse(const QByteArray &packet, ControlResponse &response);
    static bool decodeRecordStartResponse(const QByteArray &packet, ControlResponse &response);
    static bool decodeRecordStopResponse(const QByteArray &packet, ControlResponse &response);
    static bool decodePtzControlResponse(const QByteArray &packet, ControlResponse &response);

private:
    /* 在写入长度前统一校验 UTF-8/token 是否可由 uint16 表示。 */
    static bool appendLengthString(QByteArray &target, const QByteArray &bytes);
    static bool readLengthString(const QByteArray &source, int &offset, QString &value);
    static bool readLengthBytes(const QByteArray &source, int &offset, QByteArray &value);
    static QByteArray encodePacket(quint16 type, quint32 requestId, const QByteArray &body);
    static bool decodePacket(const QByteArray &packet, Packet &decoded);
};

#endif // CLIENTPROTOCOL_H
