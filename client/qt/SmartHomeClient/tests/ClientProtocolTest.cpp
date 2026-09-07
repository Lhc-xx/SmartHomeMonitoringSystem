#include <QByteArray>
#include <QString>

#include <iostream>

#include "protocol/ClientProtocol.h"

/*
 * 本测试只验证 QByteArray 中的跨端 TLV 线协议，不创建 QTcpSocket、
 * 不启动 Qt 窗口，也不会连接服务器。每个期望报文均手工构造，避免用
 * 被测编码函数反过来生成期望值而掩盖协议错误。
 */
namespace {

bool expectTrue(bool condition, const QString &message)
{
    if (condition) {
        return true;
    }

    std::cerr << message.toUtf8().constData() << std::endl;
    return false;
}

/* 将指定十六进制 TLV 作为服务端注册成功响应，正文为 errorCode 和 UTF-8 提示。 */
QByteArray registerResponse()
{
    return QByteArray::fromHex(
        "1002000100000008000000110000000000026f6b");
}

/* 构造携带 userId、token 和成功错误码的登录响应。 */
QByteArray loginResponse()
{
    return QByteArray::fromHex(
        "110200010000001300000012000000000000002a0005616263313200000000");
}

/* 构造一条设备列表响应：一个设备，三个 UTF-8 length-string 字段。 */
QByteArray deviceListResponse()
{
    return QByteArray::fromHex(
        "1202000100000025000000130000000000010000000000000007"
        "00064c6976696e6700056c6967687400066f6e6c696e65");
}

/* 构造一条录像列表响应：一个录像元数据条目，不含任何视频字节。 */
QByteArray recordQueryResponse()
{
    return QByteArray::fromHex(
        "13020001000000280000001400000000000100000000000000090000000000000007"
        "000a2f746d702f612e6d7034000173000165");
}

} // namespace

int main()
{
    /* 登录请求与注册请求均为两个 UTF-8 length-string，requestId 必须原样写入 TLV 头。 */
    const QByteArray loginRequest = ClientProtocol::encodeLoginRequest(
        QStringLiteral("alice"), QStringLiteral("secret"), 0x12);
    const QByteArray expectedLoginRequest = QByteArray::fromHex(
        "110100010000000f000000120005616c6963650006736563726574");
    if (!expectTrue(loginRequest == expectedLoginRequest,
                    QStringLiteral("登录请求 TLV 编码不符合线协议"))) {
        return 1;
    }

    /* 注册响应新增 messageLen + UTF-8 message，解析时必须保留 requestId 与错误信息。 */
    ClientProtocol::RegisterResponse registerResult;
    if (!expectTrue(ClientProtocol::decodeRegisterResponse(registerResponse(), registerResult)
                        && registerResult.requestId == 0x11
                        && registerResult.errorCode == ErrorCode::SUCCESS
                        && registerResult.message == QStringLiteral("ok"),
                    QStringLiteral("注册响应的错误码、提示或 requestId 未正确解析"))) {
        return 1;
    }

    /* 登录响应的固定 userId、可变 token 与末尾 errorCode 必须按顺序解析。 */
    ClientProtocol::LoginResponse loginResult;
    if (!expectTrue(ClientProtocol::decodeLoginResponse(loginResponse(), loginResult)
                        && loginResult.requestId == 0x12
                        && loginResult.userId == 42
                        && loginResult.token == QByteArray("abc12")
                        && loginResult.errorCode == ErrorCode::SUCCESS,
                    QStringLiteral("登录响应未正确解析 userId、token 或错误码"))) {
        return 1;
    }

    /* 设备列表响应的条目字段按 id/name/type/status 依次读取。 */
    ClientProtocol::DeviceListResponse deviceResult;
    if (!expectTrue(ClientProtocol::decodeDeviceListResponse(deviceListResponse(), deviceResult)
                        && deviceResult.requestId == 0x13
                        && deviceResult.errorCode == ErrorCode::SUCCESS
                        && deviceResult.devices.size() == 1
                        && deviceResult.devices.at(0).id == 7
                        && deviceResult.devices.at(0).name == QStringLiteral("Living")
                        && deviceResult.devices.at(0).type == QStringLiteral("light")
                        && deviceResult.devices.at(0).status == QStringLiteral("online"),
                    QStringLiteral("设备列表响应未正确解析"))) {
        return 1;
    }

    /* 录像查询响应只传输元数据，filePath 与起止时间均为 length-string。 */
    ClientProtocol::RecordQueryResponse recordResult;
    if (!expectTrue(ClientProtocol::decodeRecordQueryResponse(recordQueryResponse(), recordResult)
                        && recordResult.requestId == 0x14
                        && recordResult.errorCode == ErrorCode::SUCCESS
                        && recordResult.records.size() == 1
                        && recordResult.records.at(0).id == 9
                        && recordResult.records.at(0).deviceId == 7
                        && recordResult.records.at(0).filePath == QStringLiteral("/tmp/a.mp4"),
                    QStringLiteral("录像查询响应未正确解析"))) {
        return 1;
    }

    /* 通用拆包必须保留半包，并从粘包中每次仅消费一条完整 TLV。 */
    QByteArray receiveBuffer = loginResponse().left(10);
    ClientProtocol::Packet packet;
    ErrorCode parseError = ErrorCode::INVALID_PACKET;
    if (!expectTrue(ClientProtocol::tryTakePacket(receiveBuffer, packet, parseError)
                        == ClientProtocol::PacketState::Incomplete
                        && receiveBuffer.size() == 10,
                    QStringLiteral("半包被错误消费或错误标记为无效"))) {
        return 1;
    }
    receiveBuffer.append(loginResponse().mid(10));
    receiveBuffer.append(registerResponse());
    if (!expectTrue(ClientProtocol::tryTakePacket(receiveBuffer, packet, parseError)
                        == ClientProtocol::PacketState::Decoded
                        && packet.requestId == 0x12
                        && receiveBuffer == registerResponse(),
                    QStringLiteral("粘包中的第一条 TLV 未正确拆出"))) {
        return 1;
    }

    /* 类型、版本和超长 length 都必须安全失败，不能继续等待或越界读取。 */
    QByteArray invalidType = loginResponse();
    invalidType[1] = static_cast<char>(0x03);
    ClientProtocol::LoginResponse ignoredLogin;
    if (!expectTrue(!ClientProtocol::decodeLoginResponse(invalidType, ignoredLogin),
                    QStringLiteral("错误消息类型未被拒绝"))) {
        return 1;
    }
    QByteArray invalidTypeForFraming = loginResponse();
    invalidTypeForFraming[1] = static_cast<char>(0x03);
    if (!expectTrue(ClientProtocol::tryTakePacket(invalidTypeForFraming, packet, parseError)
                        == ClientProtocol::PacketState::Invalid
                        && parseError == ErrorCode::UNKNOWN_MESSAGE
                        && invalidTypeForFraming.isEmpty(),
                    QStringLiteral("拆包层未拒绝未知消息类型"))) {
        return 1;
    }
    QByteArray invalidVersion = loginResponse();
    invalidVersion[3] = static_cast<char>(0x02);
    if (!expectTrue(!ClientProtocol::decodeLoginResponse(invalidVersion, ignoredLogin),
                    QStringLiteral("错误协议版本未被拒绝"))) {
        return 1;
    }
    QByteArray invalidLength = QByteArray::fromHex("110200010010000100000012");
    if (!expectTrue(ClientProtocol::tryTakePacket(invalidLength, packet, parseError)
                        == ClientProtocol::PacketState::Invalid
                        && parseError == ErrorCode::BODY_TOO_LARGE
                        && invalidLength.isEmpty(),
                    QStringLiteral("超长 length 未被安全拒绝"))) {
        return 1;
    }

    /* ---- 流媒体 / 录像控制协议（角色 A 补齐）---- */

    /* 推流请求携带 length-string(streamUrl)；空串编码为 2 字节 0 长度。 */
    const QByteArray streamStartRequest = ClientProtocol::encodeStreamStartRequest(QString(), 0x21);
    if (!expectTrue(streamStartRequest == QByteArray::fromHex("1401000100000002000000210000"),
                    QStringLiteral("推流请求 TLV 编码不符合线协议"))) {
        return 1;
    }

    /* 非空 streamUrl 编码为 uint16 大端长度 + UTF-8 字节。 */
    const QByteArray streamStartWithUrl = ClientProtocol::encodeStreamStartRequest(
        QStringLiteral("rtsp://cam"), 0x25);
    if (!expectTrue(streamStartWithUrl
                        == QByteArray::fromHex("140100010000000c00000025000a727473703a2f2f63616d"),
                    QStringLiteral("推流请求 streamUrl 未按 length-string 编码"))) {
        return 1;
    }

    /* 停流请求同样无业务字段。 */
    const QByteArray streamStopRequest = ClientProtocol::encodeStreamStopRequest(0x22);
    if (!expectTrue(streamStopRequest == QByteArray::fromHex("150100010000000000000022"),
                    QStringLiteral("停流请求 TLV 编码不符合线协议"))) {
        return 1;
    }

    /* 录像开始请求携带 deviceId（8 字节大端 uint64）。 */
    const QByteArray recordStartRequest = ClientProtocol::encodeRecordStartRequest(7, 0x23);
    if (!expectTrue(recordStartRequest
                        == QByteArray::fromHex("1601000100000008000000230000000000000007"),
                    QStringLiteral("录像开始请求的 deviceId 未正确编码"))) {
        return 1;
    }

    /* 录像停止请求无业务字段。 */
    const QByteArray recordStopRequest = ClientProtocol::encodeRecordStopRequest(0x24);
    if (!expectTrue(recordStopRequest == QByteArray::fromHex("170100010000000000000024"),
                    QStringLiteral("录像停止请求 TLV 编码不符合线协议"))) {
        return 1;
    }

    /* 推流响应只含 errorCode:int32，成功码为 0。 */
    ClientProtocol::ControlResponse controlResult;
    if (!expectTrue(ClientProtocol::decodeStreamStartResponse(
                        QByteArray::fromHex("14020001000000040000002500000000"), controlResult)
                        && controlResult.requestId == 0x25
                        && controlResult.errorCode == ErrorCode::SUCCESS,
                    QStringLiteral("推流响应未正确解析 errorCode"))) {
        return 1;
    }

    /* 未授权（2005）应被正确还原；且错误类型会被拒绝。 */
    if (!expectTrue(ClientProtocol::decodeRecordStartResponse(
                        QByteArray::fromHex("160200010000000400000026000007d5"), controlResult)
                        && controlResult.errorCode == ErrorCode::UNAUTHORIZED,
                    QStringLiteral("录像开始响应的未授权错误码未正确解析"))) {
        return 1;
    }
    if (!expectTrue(!ClientProtocol::decodeStreamStartResponse(
                        QByteArray::fromHex("160200010000000400000026000007d5"), controlResult),
                    QStringLiteral("错误响应类型未被 stream 解码器拒绝"))) {
        return 1;
    }

    return 0;
}
