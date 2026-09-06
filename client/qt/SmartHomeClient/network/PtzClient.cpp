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
      m_moveActive(false)
{
}

PtzClient::~PtzClient()
{
    /* 析构前发送停止请求，避免窗口关闭时摄像头继续保持运动状态。 */
    stopMove();
}

void PtzClient::setCamera(const QUrl &webUrl, const QString &user, const QString &password)
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
    sendControl(buildControlQuery(direction, true));
}

void PtzClient::stopMove()
{
    if (!m_moveActive) {
        return;
    }
    m_moveActive = false;
    if (m_ready) {
        sendControl(buildControlQuery(Direction::Up, false));
    }
}

bool PtzClient::isReady() const
{
    return m_ready;
}

QUrlQuery PtzClient::buildControlQuery(Direction direction, bool start)
{
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("direction"), start ? directionName(direction) : QStringLiteral("stop"));
    query.addQueryItem(QStringLiteral("move"), start ? QStringLiteral("start") : QStringLiteral("stop"));
    return query;
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
