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
      m_channelId(1),
      m_ptzSpeed(4),
      m_ready(false),
      m_moveActive(false)
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
        QNetworkReply *oldReply = m_probeReply;
        m_probeReply = nullptr;
        /* 先清空成员再 abort，兼容 Qt 在 abort() 内同步派发 finished。 */
        oldReply->abort();
        oldReply->deleteLater();
    }
    if (m_controlReply != nullptr) {
        QNetworkReply *oldReply = m_controlReply;
        m_controlReply = nullptr;
        oldReply->abort();
        oldReply->deleteLater();
    }

    m_webUrl = webUrl;
    m_user = user;
    m_password = password;
    /* 设备网页 API 的通道号从 1 开始，异常配置统一钳制到第一个通道。 */
    m_channelId = qMax(1, channelId);
    /* 切换摄像头后先恢复安全默认值，能力探测成功后再采用设备返回速度。 */
    m_ptzSpeed = 4;
    m_moveActive = false;
    clearReadyState();
}

void PtzClient::setControlForwarder(
    const std::function<void(const QString &, const QString &, const QString &)> &forwarder)
{
    m_controlForwarder = forwarder;
}

void PtzClient::probe()
{
    clearReadyState();
    if (!m_webUrl.isValid() || m_webUrl.scheme().isEmpty() || m_webUrl.host().isEmpty()) {
        emit errorOccurred(QStringLiteral("球机 Web 地址无效"));
        return;
    }

    if (m_probeReply != nullptr) {
        QNetworkReply *oldReply = m_probeReply;
        m_probeReply = nullptr;
        /* 先失效旧指针，避免 abort() 的同步 finished 回调被误认为当前请求。 */
        oldReply->abort();
        oldReply->deleteLater();
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

    /* 注入转发回调后经服务器转发；否则走直连（测试/无服务器场景）。 */
    if (m_controlForwarder) {
        m_controlForwarder(m_webUrl.toString(), directionName(direction), QStringLiteral("start"));
    } else {
        sendControl(buildControlQuery(direction, true, m_channelId, m_ptzSpeed));
    }
}

void PtzClient::stopMove()
{
    if (!m_moveActive) {
        return;
    }
    m_moveActive = false;
    if (m_ready) {
        if (m_controlForwarder) {
            m_controlForwarder(m_webUrl.toString(), QStringLiteral("stop"), QStringLiteral("stop"));
        } else {
            sendControl(buildControlQuery(Direction::Up, false, m_channelId, m_ptzSpeed));
        }
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
     * XSW 球机网页端通过 value 单字母/数字编码方向：
     * 1/u/2/l/r/3/d/4 分别是八个方向，s 是停止。
     * channelId 与 speed 必须同时携带，否则设备会返回 502。
     */
    query.addQueryItem(QStringLiteral("channelId"), QString::number(qMax(1, channelId)));
    query.addQueryItem(QStringLiteral("value"), deviceValue(direction, start));
    query.addQueryItem(QStringLiteral("speed"), QString::number(qMax(1, speed)));
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
        QNetworkReply *oldReply = m_controlReply;
        m_controlReply = nullptr;
        /* 控制请求同样可能在 abort() 时同步结束，必须先标记旧请求失效。 */
        oldReply->abort();
        oldReply->deleteLater();
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
    /* probe() 可能在旧请求结束前再次发起探测；旧回调不能覆盖当前请求状态。 */
    if (reply != m_probeReply) {
        reply->deleteLater();
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
    const int reportedSpeed = object.value(QStringLiteral("ptzSpeed")).toInt();
    if (reportedSpeed > 0 && reportedSpeed <= 100) {
        /* 能力接口返回的速度用于后续每一条控制请求，避免使用失配的默认值。 */
        m_ptzSpeed = reportedSpeed;
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
    /* 只处理当前控制请求，避免已取消的旧回复误报失败。 */
    if (reply != m_controlReply) {
        reply->deleteLater();
        return;
    }
    m_controlReply = nullptr;
    const int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const QJsonDocument document = QJsonDocument::fromJson(reply->readAll());
    const QJsonObject object = document.object();
    const bool deviceRejected = document.isObject()
        && object.contains(QStringLiteral("code"))
        && object.value(QStringLiteral("code")).toInt() != 0;
    if (reply->error() != QNetworkReply::NoError
        || statusCode < 200
        || statusCode >= 300
        || deviceRejected) {
        emit errorOccurred(QStringLiteral("云台控制请求失败"));
    }
    reply->deleteLater();
}

void PtzClient::clearReadyState()
{
    m_ready = false;
    emit ptzReady(false);
}
