#include "PtzClient.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>

PtzClient::PtzClient(QObject *parent)
    : QObject(parent),
      m_manager(new QNetworkAccessManager(this)),
      m_probeReply(nullptr),
      m_controlReply(nullptr),
      m_ready(false),
      m_moveActive(false),
      m_channelId(1),
      m_ptzSpeed(4)
{
}

PtzClient::~PtzClient()
{
    /* 析构前发送停止请求，避免窗口关闭时摄像头继续保持运动状态。 */
    stopMove();
}

void PtzClient::setCamera(const QUrl &webUrl, const QString &user, const QString &password,
                          int channelId)
{
    if (m_probeReply != nullptr) {
        m_probeReply->abort();
        m_probeReply->deleteLater();
        m_probeReply = nullptr;
    }
    if (m_controlReply != nullptr) {
        m_controlReply->abort();
        m_controlReply->deleteLater();
        m_controlReply = nullptr;
    }

    m_webUrl = webUrl;
    m_user = user;
    m_password = password;
    /* 摄像头 API 使用从 1 开始的逻辑通道；非法值回退到首路。 */
    m_channelId = qMax(1, channelId);
    m_ptzSpeed = 4;
    m_moveActive = false;
    clearReadyState();
}

void PtzClient::probe()
{
    clearReadyState();
    if (!m_webUrl.isValid() || m_webUrl.scheme().isEmpty() || m_webUrl.host().isEmpty()) {
        emit errorOccurred(QStringLiteral("球机 Web 地址无效"));
        return;
    }

    if (m_probeReply != nullptr) {
        m_probeReply->abort();
        m_probeReply->deleteLater();
    }
    /* baseConf 是设备网页端公开的只读能力接口，不会触发任何物理动作。 */
    m_probeReply = m_manager->get(buildRequest(QStringLiteral("/api/ptz/baseConf")));
    connect(m_probeReply, &QNetworkReply::finished,
            this, &PtzClient::handleProbeFinished);
}

void PtzClient::startMove(Direction direction)
{
    if (!m_ready) {
        emit errorOccurred(QStringLiteral("球机尚未完成云台能力探测"));
        return;
    }

    if (m_moveActive) {
        stopMove();
    }
    m_moveActive = true;
    sendControl(buildControlQuery(direction, true, m_channelId, m_ptzSpeed));
}

void PtzClient::stopMove()
{
    if (!m_moveActive) {
        return;
    }
    m_moveActive = false;
    if (m_ready) {
        sendControl(buildControlQuery(Direction::Up, false, m_channelId, m_ptzSpeed));
    }
}

bool PtzClient::isReady() const
{
    return m_ready;
}

QUrlQuery PtzClient::buildControlQuery(Direction direction, bool start,
                                       int channelId, int speed)
{
    QUrlQuery query;
    /*
     * 设备网页端并不接受抽象的 direction/move 字段，而是要求：
     * channelId=逻辑通道、value=方向编码、speed=速度。
     * 固定字段顺序便于抓包排查，也让离线测试可以锁定真实接口契约。
     */
    query.addQueryItem(QStringLiteral("channelId"), QString::number(qMax(1, channelId)));
    query.addQueryItem(QStringLiteral("value"), deviceValue(direction, start));
    query.addQueryItem(QStringLiteral("speed"), QString::number(qBound(1, speed, 100)));
    return query;
}

QString PtzClient::deviceValue(Direction direction, bool start)
{
    if (!start) {
        return QStringLiteral("s");
    }
    switch (direction) {
    case Direction::UpLeft: return QStringLiteral("1");
    case Direction::Up: return QStringLiteral("u");
    case Direction::UpRight: return QStringLiteral("2");
    case Direction::Left: return QStringLiteral("l");
    case Direction::Right: return QStringLiteral("r");
    case Direction::DownLeft: return QStringLiteral("3");
    case Direction::Down: return QStringLiteral("d");
    case Direction::DownRight: return QStringLiteral("4");
    }
    return QStringLiteral("s");
}

QString PtzClient::directionName(Direction direction)
{
    switch (direction) {
    case Direction::Up: return QStringLiteral("up");
    case Direction::Down: return QStringLiteral("down");
    case Direction::Left: return QStringLiteral("left");
    case Direction::Right: return QStringLiteral("right");
    case Direction::UpLeft: return QStringLiteral("up-left");
    case Direction::UpRight: return QStringLiteral("up-right");
    case Direction::DownLeft: return QStringLiteral("down-left");
    case Direction::DownRight: return QStringLiteral("down-right");
    }
    return QStringLiteral("unknown");
}

QNetworkRequest PtzClient::buildRequest(const QString &path, const QUrlQuery &query) const
{
    QUrl url = endpoint(path);
    url.setQuery(query);
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::KnownHeaders::ContentTypeHeader,
                      QStringLiteral("application/json"));
    if (!m_user.isEmpty()) {
        const QByteArray credentials = (m_user + QStringLiteral(":") + m_password).toUtf8().toBase64();
        request.setRawHeader("Authorization", QByteArray("Basic ") + credentials);
    }
    return request;
}

QUrl PtzClient::endpoint(const QString &path) const
{
    QUrl result = m_webUrl;
    QString normalized = path;
    if (!normalized.startsWith(QLatin1Char('/'))) {
        normalized.prepend(QLatin1Char('/'));
    }
    result.setPath(normalized);
    result.setQuery(QUrlQuery());
    return result;
}

void PtzClient::sendControl(const QUrlQuery &query)
{
    if (m_controlReply != nullptr) {
        m_controlReply->abort();
        m_controlReply->deleteLater();
    }
    m_controlReply = m_manager->get(buildRequest(QStringLiteral("/api/ptz/control"), query));
    connect(m_controlReply, &QNetworkReply::finished,
            this, &PtzClient::handleControlFinished);
}

void PtzClient::handleProbeFinished()
{
    QNetworkReply *reply = qobject_cast<QNetworkReply *>(sender());
    if (reply == nullptr) {
        return;
    }
    m_probeReply = nullptr;

    if (reply->error() != QNetworkReply::NoError) {
        emit errorOccurred(QStringLiteral("球机能力探测失败：网络错误"));
        emit ptzReady(false);
        reply->deleteLater();
        return;
    }

    const int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const QJsonDocument document = QJsonDocument::fromJson(reply->readAll());
    const QJsonObject object = document.object();
    if (object.value(QStringLiteral("ptzSpeed")).isDouble()) {
        const int reportedSpeed = object.value(QStringLiteral("ptzSpeed")).toInt();
        if (reportedSpeed >= 1 && reportedSpeed <= 100) {
            m_ptzSpeed = reportedSpeed;
        }
    }
    const bool supported = statusCode >= 200 && statusCode < 300
        && document.isObject()
        && (object.contains(QStringLiteral("ptzSpeed")) || object.contains(QStringLiteral("steps")));
    m_ready = supported;
    emit ptzReady(supported);
    if (!supported) {
        emit errorOccurred(QStringLiteral("摄像头未返回可用的云台能力"));
    }
    reply->deleteLater();
}

void PtzClient::handleControlFinished()
{
    QNetworkReply *reply = qobject_cast<QNetworkReply *>(sender());
    if (reply == nullptr) {
        return;
    }
    m_controlReply = nullptr;
    if (reply->error() != QNetworkReply::NoError
        || reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() < 200
        || reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() >= 300) {
        emit errorOccurred(QStringLiteral("云台控制请求失败"));
    }
    reply->deleteLater();
}

void PtzClient::clearReadyState()
{
    m_ready = false;
    emit ptzReady(false);
}
