#include "ClientProtocol.h"

namespace {

const quint16 kProtocolVersion = 1;
const int kHeaderSize = 12;
const quint32 kMaxBodySize = 1024 * 1024;

/* 所有辅助读写函数仅在调用方确认边界后运行，防止 QByteArray 越界访问。 */
void appendU16(QByteArray &target, quint16 value)
{
    target.append(static_cast<char>((value >> 8) & 0xff));
    target.append(static_cast<char>(value & 0xff));
}

void appendU32(QByteArray &target, quint32 value)
{
    target.append(static_cast<char>((value >> 24) & 0xff));
    target.append(static_cast<char>((value >> 16) & 0xff));
    target.append(static_cast<char>((value >> 8) & 0xff));
    target.append(static_cast<char>(value & 0xff));
}

void appendU64(QByteArray &target, quint64 value)
{
    for (int shift = 56; shift >= 0; shift -= 8) {
        target.append(static_cast<char>((value >> shift) & 0xff));
    }
}

quint16 readU16(const QByteArray &source, int offset)
{
    return (static_cast<quint16>(static_cast<quint8>(source.at(offset))) << 8)
        | static_cast<quint8>(source.at(offset + 1));
}

quint32 readU32(const QByteArray &source, int offset)
{
    return (static_cast<quint32>(static_cast<quint8>(source.at(offset))) << 24)
        | (static_cast<quint32>(static_cast<quint8>(source.at(offset + 1))) << 16)
        | (static_cast<quint32>(static_cast<quint8>(source.at(offset + 2))) << 8)
        | static_cast<quint8>(source.at(offset + 3));
}

quint64 readU64(const QByteArray &source, int offset)
{
    quint64 value = 0;
    for (int index = 0; index < 8; ++index) {
        value = (value << 8) | static_cast<quint8>(source.at(offset + index));
    }
    return value;
}

bool hasBytes(const QByteArray &source, int offset, int count)
{
    return offset >= 0 && count >= 0 && offset <= source.size()
        && count <= source.size() - offset;
}

/* 仅允许当前 B 模块定义的消息类型进入业务层，未知类型必须在拆包层拒绝。 */
bool isKnownType(quint16 type)
{
    switch (type) {
    case ClientProtocol::RegisterRequest:
    case ClientProtocol::RegisterResponseType:
    case ClientProtocol::LoginRequest:
    case ClientProtocol::LoginResponseType:
    case ClientProtocol::DeviceListRequest:
    case ClientProtocol::DeviceListResponseType:
    case ClientProtocol::RecordQueryRequest:
    case ClientProtocol::RecordQueryResponseType:
    case ClientProtocol::StreamStartRequest:
    case ClientProtocol::StreamStartResponseType:
    case ClientProtocol::StreamStopRequest:
    case ClientProtocol::StreamStopResponseType:
    case ClientProtocol::RecordStartRequest:
    case ClientProtocol::RecordStartResponseType:
    case ClientProtocol::RecordStopRequest:
    case ClientProtocol::RecordStopResponseType:
    case ClientProtocol::PtzControlRequest:
    case ClientProtocol::PtzControlResponseType:
        return true;
    default:
        return false;
    }
}

} // namespace

bool ClientProtocol::appendLengthString(QByteArray &target, const QByteArray &bytes)
{
    /* length-string 的长度字段为 uint16，因此超过边界时拒绝整包而不是截断。 */
    if (bytes.size() > 0xffff) {
        return false;
    }
    appendU16(target, static_cast<quint16>(bytes.size()));
    target.append(bytes);
    return true;
}

bool ClientProtocol::readLengthBytes(const QByteArray &source, int &offset, QByteArray &value)
{
    if (!hasBytes(source, offset, 2)) {
        return false;
    }
    const int length = static_cast<int>(readU16(source, offset));
    offset += 2;
    if (!hasBytes(source, offset, length)) {
        return false;
    }
    value = source.mid(offset, length);
    offset += length;
    return true;
}

bool ClientProtocol::readLengthString(const QByteArray &source, int &offset, QString &value)
{
    QByteArray bytes;
    if (!readLengthBytes(source, offset, bytes)) {
        return false;
    }
    /* fromUtf8 对非法序列替换字符；线协议仍以原始边界正确性为安全前提。 */
    value = QString::fromUtf8(bytes);
    return true;
}

QByteArray ClientProtocol::encodePacket(quint16 type, quint32 requestId, const QByteArray &body)
{
    if (static_cast<quint32>(body.size()) > kMaxBodySize) {
        return QByteArray();
    }
    QByteArray packet;
    packet.reserve(kHeaderSize + body.size());
    appendU16(packet, type);
    appendU16(packet, kProtocolVersion);
    appendU32(packet, static_cast<quint32>(body.size()));
    appendU32(packet, requestId);
    packet.append(body);
    return packet;
}

QByteArray ClientProtocol::encodeRegisterRequest(const QString &username, const QString &password,
                                                 quint32 requestId)
{
    QByteArray body;
    if (!appendLengthString(body, username.toUtf8())
        || !appendLengthString(body, password.toUtf8())) {
        return QByteArray();
    }
    return encodePacket(RegisterRequest, requestId, body);
}

QByteArray ClientProtocol::encodeLoginRequest(const QString &username, const QString &password,
                                              quint32 requestId)
{
    QByteArray body;
    if (!appendLengthString(body, username.toUtf8())
        || !appendLengthString(body, password.toUtf8())) {
        return QByteArray();
    }
    return encodePacket(LoginRequest, requestId, body);
}

QByteArray ClientProtocol::encodeDeviceListRequest(quint64 userId, const QByteArray &token,
                                                   quint32 requestId)
{
    QByteArray body;
    appendU64(body, userId);
    if (!appendLengthString(body, token)) {
        return QByteArray();
    }
    return encodePacket(DeviceListRequest, requestId, body);
}

QByteArray ClientProtocol::encodeRecordQueryRequest(quint64 userId, const QByteArray &token,
                                                    quint64 deviceId, const QString &startTime,
                                                    const QString &endTime, quint32 requestId)
{
    QByteArray body;
    appendU64(body, userId);
    if (!appendLengthString(body, token)) {
        return QByteArray();
    }
    appendU64(body, deviceId);
    if (!appendLengthString(body, startTime.toUtf8())
        || !appendLengthString(body, endTime.toUtf8())) {
        return QByteArray();
    }
    return encodePacket(RecordQueryRequest, requestId, body);
}

QByteArray ClientProtocol::encodeStreamStartRequest(const QString &streamUrl, quint32 requestId)
{
    /* payload = length-string(streamUrl)；空串对应服务端 Mock 源。 */
    QByteArray body;
    if (!appendLengthString(body, streamUrl.toUtf8())) {
        return QByteArray();
    }
    return encodePacket(StreamStartRequest, requestId, body);
}

QByteArray ClientProtocol::encodeStreamStopRequest(quint32 requestId)
{
    return encodePacket(StreamStopRequest, requestId, QByteArray());
}

QByteArray ClientProtocol::encodeRecordStartRequest(quint64 deviceId, quint32 requestId)
{
    QByteArray body;
    appendU64(body, deviceId);
    return encodePacket(RecordStartRequest, requestId, body);
}

QByteArray ClientProtocol::encodeRecordStopRequest(quint32 requestId)
{
    return encodePacket(RecordStopRequest, requestId, QByteArray());
}

QByteArray ClientProtocol::encodePtzControlRequest(const QString &cameraUrl,
                                                   const QString &direction,
                                                   const QString &move,
                                                   quint32 requestId)
{
    QByteArray body;
    if (!appendLengthString(body, cameraUrl.toUtf8())
        || !appendLengthString(body, direction.toUtf8())
        || !appendLengthString(body, move.toUtf8())) {
        return QByteArray();
    }
    return encodePacket(PtzControlRequest, requestId, body);
}

ClientProtocol::PacketState ClientProtocol::tryTakePacket(QByteArray &receiveBuffer,
                                                           Packet &packet,
                                                           ErrorCode &errorCode)
{
    if (receiveBuffer.size() < kHeaderSize) {
        return PacketState::Incomplete;
    }
    /* 版本和类型位于固定头部，先校验可以避免非法包伪造超长 body 造成无意义等待。 */
    const quint16 messageType = readU16(receiveBuffer, 0);
    const quint16 version = readU16(receiveBuffer, 2);
    if (version != kProtocolVersion) {
        receiveBuffer.clear();
        errorCode = ErrorCode::UNSUPPORTED_VERSION;
        return PacketState::Invalid;
    }
    if (!isKnownType(messageType)) {
        receiveBuffer.clear();
        errorCode = ErrorCode::UNKNOWN_MESSAGE;
        return PacketState::Invalid;
    }
    const quint32 bodyLength = readU32(receiveBuffer, 4);
    if (bodyLength > kMaxBodySize) {
        receiveBuffer.clear();
        errorCode = ErrorCode::BODY_TOO_LARGE;
        return PacketState::Invalid;
    }
    const qint64 totalLength = kHeaderSize + static_cast<qint64>(bodyLength);
    if (receiveBuffer.size() < totalLength) {
        return PacketState::Incomplete;
    }
    const QByteArray complete = receiveBuffer.left(static_cast<int>(totalLength));
    receiveBuffer.remove(0, static_cast<int>(totalLength));
    if (!decodePacket(complete, packet)) {
        errorCode = ErrorCode::INVALID_PACKET;
        return PacketState::Invalid;
    }
    errorCode = ErrorCode::SUCCESS;
    return PacketState::Decoded;
}

bool ClientProtocol::decodePacket(const QByteArray &packet, Packet &decoded)
{
    if (packet.size() < kHeaderSize) {
        return false;
    }
    const quint32 bodyLength = readU32(packet, 4);
    if (bodyLength > kMaxBodySize
        || packet.size() != kHeaderSize + static_cast<int>(bodyLength)
        || readU16(packet, 2) != kProtocolVersion
        || !isKnownType(readU16(packet, 0))) {
        return false;
    }
    decoded.type = readU16(packet, 0);
    decoded.version = kProtocolVersion;
    decoded.requestId = readU32(packet, 8);
    decoded.body = packet.mid(kHeaderSize);
    decoded.raw = packet;
    return true;
}

bool ClientProtocol::decodeRegisterResponse(const QByteArray &packet, RegisterResponse &response)
{
    Packet decoded;
    if (!decodePacket(packet, decoded) || decoded.type != RegisterResponseType) {
        return false;
    }
    int offset = 0;
    if (!hasBytes(decoded.body, offset, 4)) {
        return false;
    }
    response.requestId = decoded.requestId;
    response.errorCode = static_cast<ErrorCode>(static_cast<qint32>(readU32(decoded.body, offset)));
    offset += 4;
    /* 兼容旧服务端仅返回 errorCode 的 4 字节响应；新协议仍优先解析 message。 */
    if (offset == decoded.body.size()) {
        response.message.clear();
        return true;
    }
    if (!readLengthString(decoded.body, offset, response.message)) {
        return false;
    }
    return offset == decoded.body.size();
}

bool ClientProtocol::decodeLoginResponse(const QByteArray &packet, LoginResponse &response)
{
    Packet decoded;
    if (!decodePacket(packet, decoded) || decoded.type != LoginResponseType) {
        return false;
    }
    int offset = 0;
    if (!hasBytes(decoded.body, offset, 8)) {
        return false;
    }
    response.requestId = decoded.requestId;
    response.userId = readU64(decoded.body, offset);
    offset += 8;
    if (!readLengthBytes(decoded.body, offset, response.token)
        || !hasBytes(decoded.body, offset, 4)) {
        return false;
    }
    response.errorCode = static_cast<ErrorCode>(static_cast<qint32>(readU32(decoded.body, offset)));
    offset += 4;
    return offset == decoded.body.size();
}

bool ClientProtocol::decodeDeviceListResponse(const QByteArray &packet, DeviceListResponse &response)
{
    Packet decoded;
    if (!decodePacket(packet, decoded) || decoded.type != DeviceListResponseType) {
        return false;
    }
    int offset = 0;
    if (!hasBytes(decoded.body, offset, 6)) {
        return false;
    }
    response.requestId = decoded.requestId;
    response.errorCode = static_cast<ErrorCode>(static_cast<qint32>(readU32(decoded.body, offset)));
    offset += 4;
    const quint16 count = readU16(decoded.body, offset);
    offset += 2;
    response.devices.clear();
    for (quint16 index = 0; index < count; ++index) {
        DeviceInfo item;
        if (!hasBytes(decoded.body, offset, 8)) {
            return false;
        }
        item.id = readU64(decoded.body, offset);
        offset += 8;
        if (!readLengthString(decoded.body, offset, item.name)
            || !readLengthString(decoded.body, offset, item.type)
            || !readLengthString(decoded.body, offset, item.status)) {
            return false;
        }
        response.devices.append(item);
    }
    return offset == decoded.body.size();
}

bool ClientProtocol::decodeRecordQueryResponse(const QByteArray &packet, RecordQueryResponse &response)
{
    Packet decoded;
    if (!decodePacket(packet, decoded) || decoded.type != RecordQueryResponseType) {
        return false;
    }
    int offset = 0;
    if (!hasBytes(decoded.body, offset, 6)) {
        return false;
    }
    response.requestId = decoded.requestId;
    response.errorCode = static_cast<ErrorCode>(static_cast<qint32>(readU32(decoded.body, offset)));
    offset += 4;
    const quint16 count = readU16(decoded.body, offset);
    offset += 2;
    response.records.clear();
    for (quint16 index = 0; index < count; ++index) {
        RecordInfo item;
        if (!hasBytes(decoded.body, offset, 16)) {
            return false;
        }
        item.id = readU64(decoded.body, offset);
        item.deviceId = readU64(decoded.body, offset + 8);
        offset += 16;
        if (!readLengthString(decoded.body, offset, item.filePath)
            || !readLengthString(decoded.body, offset, item.startTime)
            || !readLengthString(decoded.body, offset, item.endTime)) {
            return false;
        }
        response.records.append(item);
    }
    return offset == decoded.body.size();
}

bool ClientProtocol::decodeStreamStartResponse(const QByteArray &packet, ControlResponse &response)
{
    Packet decoded;
    if (!decodePacket(packet, decoded) || decoded.type != StreamStartResponseType) {
        return false;
    }
    int offset = 0;
    if (!hasBytes(decoded.body, offset, 4)) {
        return false;
    }
    response.requestId = decoded.requestId;
    response.errorCode = static_cast<ErrorCode>(static_cast<qint32>(readU32(decoded.body, offset)));
    offset += 4;
    return offset == decoded.body.size();
}

bool ClientProtocol::decodeStreamStopResponse(const QByteArray &packet, ControlResponse &response)
{
    Packet decoded;
    if (!decodePacket(packet, decoded) || decoded.type != StreamStopResponseType) {
        return false;
    }
    int offset = 0;
    if (!hasBytes(decoded.body, offset, 4)) {
        return false;
    }
    response.requestId = decoded.requestId;
    response.errorCode = static_cast<ErrorCode>(static_cast<qint32>(readU32(decoded.body, offset)));
    offset += 4;
    return offset == decoded.body.size();
}

bool ClientProtocol::decodeRecordStartResponse(const QByteArray &packet, ControlResponse &response)
{
    Packet decoded;
    if (!decodePacket(packet, decoded) || decoded.type != RecordStartResponseType) {
        return false;
    }
    int offset = 0;
    if (!hasBytes(decoded.body, offset, 4)) {
        return false;
    }
    response.requestId = decoded.requestId;
    response.errorCode = static_cast<ErrorCode>(static_cast<qint32>(readU32(decoded.body, offset)));
    offset += 4;
    return offset == decoded.body.size();
}

bool ClientProtocol::decodeRecordStopResponse(const QByteArray &packet, ControlResponse &response)
{
    Packet decoded;
    if (!decodePacket(packet, decoded) || decoded.type != RecordStopResponseType) {
        return false;
    }
    int offset = 0;
    if (!hasBytes(decoded.body, offset, 4)) {
        return false;
    }
    response.requestId = decoded.requestId;
    response.errorCode = static_cast<ErrorCode>(static_cast<qint32>(readU32(decoded.body, offset)));
    offset += 4;
    return offset == decoded.body.size();
}

bool ClientProtocol::decodePtzControlResponse(const QByteArray &packet, ControlResponse &response)
{
    Packet decoded;
    if (!decodePacket(packet, decoded) || decoded.type != PtzControlResponseType) {
        return false;
    }
    int offset = 0;
    if (!hasBytes(decoded.body, offset, 4)) {
        return false;
    }
    response.requestId = decoded.requestId;
    response.errorCode = static_cast<ErrorCode>(static_cast<qint32>(readU32(decoded.body, offset)));
    offset += 4;
    return offset == decoded.body.size();
}
