#include "UserService.h"

#include "network/TcpClient.h"

#include <QTimer>

UserService::UserService(TcpClient *tcpClient, QObject *parent)
    : QObject(parent)
    , m_tcpClient(tcpClient)
    , m_pending(PendingRequest::None)
    , m_pendingRequestId(0)
    , m_nextRequestId(1)
    , m_userId(0)
    , m_requestTimer(new QTimer(this))
    , m_requestTimeoutMs(5000)
{
    /* UserService 只订阅 TcpClient 的高层信号，始终不直接触碰 QTcpSocket。 */
    if (m_tcpClient != nullptr) {
        connect(m_tcpClient, &TcpClient::dataReceived, this, &UserService::onDataReceived);
        connect(m_tcpClient, &TcpClient::errorOccurred, this, &UserService::onTcpError);
        connect(m_tcpClient, &TcpClient::disconnected, this, &UserService::onDisconnected);
    }

    m_requestTimer->setSingleShot(true);
    connect(m_requestTimer, &QTimer::timeout, this, &UserService::onRequestTimeout);
}

quint32 UserService::nextRequestId()
{
    /* 跳过零值并处理 uint32 回绕，保证一次等待操作有可分流的非零标识。 */
    const quint32 result = m_nextRequestId;
    ++m_nextRequestId;
    if (m_nextRequestId == 0) {
        m_nextRequestId = 1;
    }
    return result == 0 ? nextRequestId() : result;
}

bool UserService::beginRequest(PendingRequest type, const QByteArray &packet,
                               quint32 requestId, const QString &actionName)
{
    if (m_pending != PendingRequest::None) {
        emit requestFailed(QStringLiteral("%1失败：仍在等待上一条请求响应。").arg(actionName));
        return false;
    }
    if (m_tcpClient == nullptr) {
        emit requestFailed(QStringLiteral("%1失败：网络服务尚未初始化。").arg(actionName));
        return false;
    }
    if (packet.isEmpty()) {
        emit requestFailed(QStringLiteral("%1失败：请求字段长度超过协议限制。").arg(actionName));
        return false;
    }
    m_pending = type;
    m_pendingRequestId = requestId;
    m_tcpClient->sendData(packet);
    m_requestTimer->start(m_requestTimeoutMs);
    return true;
}

void UserService::registerUser(const QString &username, const QString &password)
{
    if (username.trimmed().isEmpty() || password.isEmpty()) {
        emit registerFailed(QStringLiteral("用户名和密码不能为空。"));
        return;
    }
    const quint32 requestId = nextRequestId();
    const QByteArray packet = ClientProtocol::encodeRegisterRequest(username.trimmed(), password, requestId);
    if (!beginRequest(PendingRequest::Register, packet, requestId, QStringLiteral("注册"))) {
        emit registerFailed(QStringLiteral("注册请求未发送。"));
    }
}

void UserService::loginUser(const QString &username, const QString &password)
{
    if (username.trimmed().isEmpty() || password.isEmpty()) {
        emit loginFailed(QStringLiteral("用户名和密码不能为空。"));
        return;
    }
    const quint32 requestId = nextRequestId();
    const QByteArray packet = ClientProtocol::encodeLoginRequest(username.trimmed(), password, requestId);
    if (!beginRequest(PendingRequest::Login, packet, requestId, QStringLiteral("登录"))) {
        emit loginFailed(QStringLiteral("登录请求未发送。"));
    }
}

bool UserService::canStartAuthenticatedRequest(const QString &actionName)
{
    if (m_userId == 0 || m_token.isEmpty()) {
        emit requestFailed(QStringLiteral("%1失败：请先成功登录。").arg(actionName));
        return false;
    }
    return true;
}

void UserService::requestDeviceList()
{
    if (!canStartAuthenticatedRequest(QStringLiteral("获取设备列表"))) return;
    const quint32 requestId = nextRequestId();
    beginRequest(PendingRequest::DeviceList,
                 ClientProtocol::encodeDeviceListRequest(m_userId, m_token, requestId),
                 requestId, QStringLiteral("获取设备列表"));
}

void UserService::requestRecordQuery(quint64 deviceId, const QString &startTime,
                                     const QString &endTime)
{
    if (!canStartAuthenticatedRequest(QStringLiteral("查询录像"))) return;
    const quint32 requestId = nextRequestId();
    beginRequest(PendingRequest::RecordQuery,
                 ClientProtocol::encodeRecordQueryRequest(m_userId, m_token, deviceId,
                                                          startTime, endTime, requestId),
                 requestId, QStringLiteral("查询录像"));
}

void UserService::onDataReceived(const QByteArray &data)
{
    m_receiveBuffer.append(data);
    while (!m_receiveBuffer.isEmpty()) {
        ClientProtocol::Packet packet;
        ErrorCode parseError = ErrorCode::INVALID_PACKET;
        const ClientProtocol::PacketState state = ClientProtocol::tryTakePacket(
            m_receiveBuffer, packet, parseError);
        if (state == ClientProtocol::PacketState::Incomplete) return;
        if (state == ClientProtocol::PacketState::Invalid) {
            failPending(QStringLiteral("服务端响应协议格式错误（%1）。")
                        .arg(static_cast<qint32>(parseError)));
            return;
        }
        /* 串行模型下，仅处理当前等待 requestId；其他完整包已安全消费但不会覆盖状态。 */
        if (m_pending == PendingRequest::None || packet.requestId != m_pendingRequestId) continue;

        m_requestTimer->stop();

        if (m_pending == PendingRequest::Register) {
            ClientProtocol::RegisterResponse response;
            if (!ClientProtocol::decodeRegisterResponse(packet.raw, response)) {
                failPending(QStringLiteral("注册响应协议格式错误。"));
            } else if (response.errorCode == ErrorCode::SUCCESS) {
                m_pending = PendingRequest::None;
                m_pendingRequestId = 0;
                emit registerSuccess();
            } else {
                const QString reason = response.message.isEmpty()
                    ? errorCodeMessage(response.errorCode, QStringLiteral("注册"))
                    : response.message;
                failPending(reason);
            }
        } else if (m_pending == PendingRequest::Login) {
            ClientProtocol::LoginResponse response;
            if (!ClientProtocol::decodeLoginResponse(packet.raw, response)) {
                failPending(QStringLiteral("登录响应协议格式错误。"));
            } else if (response.errorCode == ErrorCode::SUCCESS
                       && response.userId != 0 && !response.token.isEmpty()) {
                /* token 只保存在成员变量；本处不输出、不序列化，也不交给 UI。 */
                m_userId = response.userId;
                m_token = response.token;
                m_pending = PendingRequest::None;
                m_pendingRequestId = 0;
                emit loginSuccess(m_userId);
            } else {
                failPending(errorCodeMessage(response.errorCode, QStringLiteral("登录")));
            }
        } else if (m_pending == PendingRequest::DeviceList) {
            ClientProtocol::DeviceListResponse response;
            if (!ClientProtocol::decodeDeviceListResponse(packet.raw, response)) {
                failPending(QStringLiteral("设备列表响应协议格式错误。"));
            } else if (response.errorCode == ErrorCode::SUCCESS) {
                m_pending = PendingRequest::None;
                m_pendingRequestId = 0;
                emit deviceListReceived(response.devices);
            } else {
                failPending(errorCodeMessage(response.errorCode, QStringLiteral("获取设备列表")));
            }
        } else if (m_pending == PendingRequest::RecordQuery) {
            ClientProtocol::RecordQueryResponse response;
            if (!ClientProtocol::decodeRecordQueryResponse(packet.raw, response)) {
                failPending(QStringLiteral("录像查询响应协议格式错误。"));
            } else if (response.errorCode == ErrorCode::SUCCESS) {
                m_pending = PendingRequest::None;
                m_pendingRequestId = 0;
                emit recordListReceived(response.records);
            } else {
                failPending(errorCodeMessage(response.errorCode, QStringLiteral("查询录像")));
            }
        }
    }
}

void UserService::onTcpError(const QString &message)
{
    if (m_pending != PendingRequest::None) failPending(QStringLiteral("网络错误：%1").arg(message));
}

void UserService::onDisconnected()
{
    if (m_pending != PendingRequest::None) failPending(QStringLiteral("与服务器的连接已断开。"));
}

void UserService::setRequestTimeout(int ms)
{
    m_requestTimeoutMs = ms < 0 ? 0 : ms;
}

void UserService::onRequestTimeout()
{
    if (m_pending != PendingRequest::None) {
        failPending(QStringLiteral("请求超时：服务器未在 %1 毫秒内响应。").arg(m_requestTimeoutMs));
    }
}

void UserService::failPending(const QString &reason)
{
    m_requestTimer->stop();
    const PendingRequest previous = m_pending;
    m_pending = PendingRequest::None;
    m_pendingRequestId = 0;
    if (previous == PendingRequest::Register) emit registerFailed(reason);
    else if (previous == PendingRequest::Login) emit loginFailed(reason);
    else emit requestFailed(reason);
}

QString UserService::errorCodeMessage(ErrorCode errorCode, const QString &actionName) const
{
    if (errorCode == ErrorCode::SUCCESS) return QStringLiteral("%1成功。").arg(actionName);
    return QStringLiteral("%1失败：服务端错误码 %2。").arg(actionName)
        .arg(static_cast<qint32>(errorCode));
}
