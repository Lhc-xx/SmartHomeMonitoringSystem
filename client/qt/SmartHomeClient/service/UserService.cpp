#include "UserService.h"

#include "network/TcpClient.h"
#include "protocol/media_packet.h"

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
    , m_dispatchScheduled(false)
{
    /* UserService 只订阅 TcpClient 的高层信号，始终不直接触碰 QTcpSocket。 */
    if (m_tcpClient != nullptr) {
        connect(m_tcpClient, &TcpClient::dataReceived, this, &UserService::onDataReceived);
        connect(m_tcpClient, &TcpClient::connected, this, &UserService::onConnected);
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
    /*
     * TcpClient 的发送失败信号可能在 sendData() 内同步到达并清空 pending。
     * 只有确认状态仍属于本次请求才启动定时器，避免失败后留下“幽灵”超时。
     */
    if (m_pending == type && m_pendingRequestId == requestId) {
        m_requestTimer->start(m_requestTimeoutMs);
    }
    return true;
}

void UserService::enqueueRequest(const QueuedRequest &request)
{
    if (m_tcpClient == nullptr) {
        emit requestFailed(QStringLiteral("%1失败：网络服务尚未初始化。")
                           .arg(request.actionName));
        return;
    }
    if (request.packet.isEmpty()) {
        emit requestFailed(QStringLiteral("%1失败：请求字段长度超过协议限制。")
                           .arg(request.actionName));
        return;
    }

    /* 没有等待项时立即发送；有等待项时进入同一条 requestId 串行队列。 */
    if (m_pending == PendingRequest::None && m_requestQueue.isEmpty()) {
        if (!m_tcpClient->isConnected()) {
            /*
             * QTcpSocket 的 connectToHost 是异步操作，用户可能在窗口刚显示时
             * 立即点击登录/注册。先保存完整 TLV，等 connected 信号到达后再发送，
             * 从源头消除“TCP 连接尚未建立”的竞态，而不是让 UI 重复点击。
             */
            m_requestQueue.append(request);
            emit requestWaiting(QStringLiteral("正在连接服务器，请稍候…"));
            return;
        }
        beginRequest(request.type, request.packet, request.requestId, request.actionName);
        return;
    }

    if (request.type == PendingRequest::PtzControl) {
        /*
         * 云台按键是“状态”而非可累计事件：连续移动只保留最新动作，
         * 特别是 stop 必须抢到队头，保证释放按键后不会继续转动。
         */
        for (int i = m_requestQueue.size() - 1; i >= 0; --i) {
            if (m_requestQueue.at(i).type == PendingRequest::PtzControl) {
                m_requestQueue.removeAt(i);
            }
        }
        if (request.isPtzStop) {
            m_requestQueue.prepend(request);
        } else {
            m_requestQueue.append(request);
        }
    } else {
        m_requestQueue.append(request);
    }
}

void UserService::scheduleNextRequest()
{
    if (m_dispatchScheduled || m_pending != PendingRequest::None
        || m_requestQueue.isEmpty()) {
        return;
    }
    m_dispatchScheduled = true;
    /* 下一事件循环再发送，避免响应信号槽重入当前解析栈。 */
    QTimer::singleShot(0, this, &UserService::dispatchNextRequest);
}

void UserService::dispatchNextRequest()
{
    m_dispatchScheduled = false;
    if (m_pending != PendingRequest::None || m_requestQueue.isEmpty()) {
        return;
    }
    const QueuedRequest request = m_requestQueue.takeFirst();
    beginRequest(request.type, request.packet, request.requestId, request.actionName);
    /* 传输层同步失败时 failPending 已经安排下一次；这里补齐异常路径。 */
    if (m_pending == PendingRequest::None && !m_requestQueue.isEmpty()) {
        scheduleNextRequest();
    }
}

void UserService::registerUser(const QString &username, const QString &password)
{
    if (username.trimmed().isEmpty() || password.isEmpty()) {
        emit registerFailed(QStringLiteral("用户名和密码不能为空。"));
        return;
    }
    const quint32 requestId = nextRequestId();
    const QByteArray packet = ClientProtocol::encodeRegisterRequest(username.trimmed(), password, requestId);
    enqueueRequest(QueuedRequest{
        PendingRequest::Register,
        packet,
        requestId,
        QStringLiteral("注册"),
        false
    });
}

void UserService::loginUser(const QString &username, const QString &password)
{
    if (username.trimmed().isEmpty() || password.isEmpty()) {
        emit loginFailed(QStringLiteral("用户名和密码不能为空。"));
        return;
    }
    const quint32 requestId = nextRequestId();
    const QByteArray packet = ClientProtocol::encodeLoginRequest(username.trimmed(), password, requestId);
    enqueueRequest(QueuedRequest{
        PendingRequest::Login,
        packet,
        requestId,
        QStringLiteral("登录"),
        false
    });
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
    enqueueRequest(QueuedRequest{
        PendingRequest::DeviceList,
        ClientProtocol::encodeDeviceListRequest(m_userId, m_token, requestId),
        requestId,
        QStringLiteral("获取设备列表"),
        false
    });
}

void UserService::requestRecordQuery(quint64 deviceId, const QString &startTime,
                                     const QString &endTime)
{
    if (!canStartAuthenticatedRequest(QStringLiteral("查询录像"))) return;
    const quint32 requestId = nextRequestId();
    enqueueRequest(QueuedRequest{
        PendingRequest::RecordQuery,
        ClientProtocol::encodeRecordQueryRequest(m_userId, m_token, deviceId,
                                                  startTime, endTime, requestId),
        requestId,
        QStringLiteral("查询录像"),
        false
    });
}

void UserService::startStream(const QString &streamUrl)
{
    if (!canStartAuthenticatedRequest(QStringLiteral("推流"))) return;
    const quint32 requestId = nextRequestId();
    enqueueRequest(QueuedRequest{
        PendingRequest::StreamStart,
        ClientProtocol::encodeStreamStartRequest(streamUrl, requestId),
        requestId,
        QStringLiteral("推流"),
        false
    });
}

void UserService::stopStream()
{
    if (!canStartAuthenticatedRequest(QStringLiteral("停流"))) return;
    const quint32 requestId = nextRequestId();
    enqueueRequest(QueuedRequest{
        PendingRequest::StreamStop,
        ClientProtocol::encodeStreamStopRequest(requestId),
        requestId,
        QStringLiteral("停流"),
        false
    });
}

void UserService::sendPtzControl(const QString &cameraUrl, const QString &direction,
                                 const QString &move)
{
    if (!canStartAuthenticatedRequest(QStringLiteral("云台控制"))) return;
    const quint32 requestId = nextRequestId();
    enqueueRequest(QueuedRequest{
        PendingRequest::PtzControl,
        ClientProtocol::encodePtzControlRequest(cameraUrl, direction, move, requestId),
        requestId,
        QStringLiteral("云台控制"),
        move == QStringLiteral("stop")
    });
}

void UserService::startRecording(quint64 deviceId)
{
    if (!canStartAuthenticatedRequest(QStringLiteral("开始录像"))) return;
    const quint32 requestId = nextRequestId();
    enqueueRequest(QueuedRequest{
        PendingRequest::RecordStart,
        ClientProtocol::encodeRecordStartRequest(deviceId, requestId),
        requestId,
        QStringLiteral("开始录像"),
        false
    });
}

void UserService::stopRecording()
{
    if (!canStartAuthenticatedRequest(QStringLiteral("停止录像"))) return;
    const quint32 requestId = nextRequestId();
    enqueueRequest(QueuedRequest{
        PendingRequest::RecordStop,
        ClientProtocol::encodeRecordStopRequest(requestId),
        requestId,
        QStringLiteral("停止录像"),
        false
    });
}

void UserService::onDataReceived(const QByteArray &data)
{
    m_receiveBuffer.append(data);
    while (!m_receiveBuffer.isEmpty()) {
        /*
         * 1) 先判断开头是不是媒体帧：推流后服务端把 MediaPacket 帧和 TLV 响应
         *    混在同一条连接上，这里用帧长前缀 + 魔数把媒体帧切出来交给解码器。
         *    TLV 消息的 type 恒 >= 0x1001，按帧长解释会超过上限，不会误判。
         */
        {
            uint32_t frameLen = 0;
            const uint8_t *raw = reinterpret_cast<const uint8_t *>(m_receiveBuffer.constData());
            if (smart_home::protocol::MediaPacketSerializer::peekFrameLength(
                    raw, static_cast<size_t>(m_receiveBuffer.size()), frameLen)) {
                if (m_receiveBuffer.size() < static_cast<int>(frameLen)) {
                    return;  // 媒体帧未收全，等下一批字节
                }
                emit mediaFrameReceived(m_receiveBuffer.left(static_cast<int>(frameLen)));
                m_receiveBuffer.remove(0, static_cast<int>(frameLen));
                continue;
            }
        }

        /* 2) 否则按 TLV 解析。 */
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
                completePending();
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
                completePending();
                emit loginSuccess(m_userId);
            } else {
                failPending(errorCodeMessage(response.errorCode, QStringLiteral("登录")));
            }
        } else if (m_pending == PendingRequest::DeviceList) {
            ClientProtocol::DeviceListResponse response;
            if (!ClientProtocol::decodeDeviceListResponse(packet.raw, response)) {
                failPending(QStringLiteral("设备列表响应协议格式错误。"));
            } else if (response.errorCode == ErrorCode::SUCCESS) {
                completePending();
                emit deviceListReceived(response.devices);
            } else {
                failPending(errorCodeMessage(response.errorCode, QStringLiteral("获取设备列表")));
            }
        } else if (m_pending == PendingRequest::RecordQuery) {
            ClientProtocol::RecordQueryResponse response;
            if (!ClientProtocol::decodeRecordQueryResponse(packet.raw, response)) {
                failPending(QStringLiteral("录像查询响应协议格式错误。"));
            } else if (response.errorCode == ErrorCode::SUCCESS) {
                completePending();
                emit recordListReceived(response.records);
            } else {
                failPending(errorCodeMessage(response.errorCode, QStringLiteral("查询录像")));
            }
        } else if (m_pending == PendingRequest::StreamStart) {
            ClientProtocol::ControlResponse response;
            if (!ClientProtocol::decodeStreamStartResponse(packet.raw, response)) {
                failPending(QStringLiteral("推流响应协议格式错误。"));
            } else if (response.errorCode == ErrorCode::SUCCESS) {
                completePending();
                emit streamStarted();
            } else {
                failPending(errorCodeMessage(response.errorCode, QStringLiteral("推流")));
            }
        } else if (m_pending == PendingRequest::StreamStop) {
            ClientProtocol::ControlResponse response;
            if (!ClientProtocol::decodeStreamStopResponse(packet.raw, response)) {
                failPending(QStringLiteral("停流响应协议格式错误。"));
            } else if (response.errorCode == ErrorCode::SUCCESS) {
                completePending();
                emit streamStopped();
            } else {
                failPending(errorCodeMessage(response.errorCode, QStringLiteral("停流")));
            }
        } else if (m_pending == PendingRequest::PtzControl) {
            ClientProtocol::ControlResponse response;
            if (!ClientProtocol::decodePtzControlResponse(packet.raw, response)) {
                failPending(QStringLiteral("云台控制响应协议格式错误。"));
            } else if (response.errorCode == ErrorCode::SUCCESS) {
                completePending();
            } else {
                failPending(errorCodeMessage(response.errorCode, QStringLiteral("云台控制")));
            }
        } else if (m_pending == PendingRequest::RecordStart) {
            ClientProtocol::ControlResponse response;
            if (!ClientProtocol::decodeRecordStartResponse(packet.raw, response)) {
                failPending(QStringLiteral("开始录像响应协议格式错误。"));
            } else if (response.errorCode == ErrorCode::SUCCESS) {
                completePending();
                emit recordingStarted();
            } else {
                failPending(errorCodeMessage(response.errorCode, QStringLiteral("开始录像")));
            }
        } else if (m_pending == PendingRequest::RecordStop) {
            ClientProtocol::ControlResponse response;
            if (!ClientProtocol::decodeRecordStopResponse(packet.raw, response)) {
                failPending(QStringLiteral("停止录像响应协议格式错误。"));
            } else if (response.errorCode == ErrorCode::SUCCESS) {
                completePending();
                emit recordingStopped();
            } else {
                failPending(errorCodeMessage(response.errorCode, QStringLiteral("停止录像")));
            }
        }
    }
}

void UserService::onTcpError(const QString &message)
{
    if (m_pending != PendingRequest::None) failPending(QStringLiteral("网络错误：%1").arg(message));
    else if (!m_requestQueue.isEmpty()) {
        /* 连接失败时保留排队的认证请求，TcpClient 会自动重连后继续发送。 */
        emit requestWaiting(QStringLiteral("网络连接暂未建立，正在自动重试…"));
    }
}

void UserService::onConnected()
{
    /* 连接恢复后，发送此前在握手阶段缓存的登录/注册 TLV。 */
    scheduleNextRequest();
}

void UserService::onDisconnected()
{
    /*
     * TCP 连接一旦断开，当前连接所承载的认证上下文就不能继续使用。
     * 即使服务端 token 尚未过期，也必须要求用户在新连接上重新登录，
     * 避免资源请求携带旧 userId/token 并产生难以判断的越权或过期错误。
     */
    /* 连接上下文已失效，旧队列中的 token 和 PTZ 状态不能跨连接重放。 */
    /* 未登录阶段的认证请求可以跨一次短暂断线保留，待自动重连后发送。 */
    const bool waitingForAuthentication = m_userId == 0 && !m_requestQueue.isEmpty();
    if (!waitingForAuthentication) {
        m_requestQueue.clear();
    }
    m_dispatchScheduled = false;
    if (m_pending != PendingRequest::None) {
        failPending(QStringLiteral("与服务器的连接已断开。"));
    }
    m_userId = 0;
    m_token.clear();
    m_receiveBuffer.clear();
    if (waitingForAuthentication) {
        emit requestWaiting(QStringLiteral("服务器连接已断开，正在重新连接…"));
    }
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

void UserService::completePending()
{
    m_requestTimer->stop();
    m_pending = PendingRequest::None;
    m_pendingRequestId = 0;
    scheduleNextRequest();
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
    scheduleNextRequest();
}

QString UserService::errorCodeMessage(ErrorCode errorCode, const QString &actionName) const
{
    if (errorCode == ErrorCode::SUCCESS) return QStringLiteral("%1成功。").arg(actionName);
    return QStringLiteral("%1失败：服务端错误码 %2。").arg(actionName)
        .arg(static_cast<qint32>(errorCode));
}
