#ifndef PTZCLIENT_H
#define PTZCLIENT_H

#include <QNetworkRequest>
#include <QObject>
#include <QUrl>
#include <QUrlQuery>

#include <functional>

class QNetworkAccessManager;
class QNetworkReply;

/*
 * PtzClient 负责把 Qt 工作台的云台操作适配为摄像头网页 API 请求。
 *
 * 默认走摄像头 Web 服务直连（探针 /api/ptz/baseConf、控制 /api/ptz/control）。
 * 若通过 setControlForwarder() 注入转发回调，则云台控制改经服务器 TLV 转发
 * （PTZ_CONTROL_REQUEST），能力探测仍走直连只读接口。程序必须先完成探测，
 * 方向按钮在收到成功响应前保持禁用；停止请求在释放、失焦和析构时补发。
 */
class PtzClient : public QObject
{
    Q_OBJECT

public:
    enum class Direction {
        Up,
        Down,
        Left,
        Right,
        UpLeft,
        UpRight,
        DownLeft,
        DownRight
    };

    explicit PtzClient(QObject *parent = nullptr);
    ~PtzClient() override;

    /* 摄像头网页端通道从 1 开始；RTSP chn=0 对应默认 channelId=1。 */
    void setCamera(const QUrl &webUrl, const QString &user, const QString &password,
                   int channelId = 1);
    /* 注入转发回调后，控制请求交给服务器转发（参数：cameraUrl, direction, move）。 */
    void setControlForwarder(const std::function<void(const QString &, const QString &, const QString &)> &forwarder);
    void probe();
    void startMove(Direction direction);
    void stopMove();

    bool isReady() const;

    /* 集中定义设备 API 的方向和开始/停止参数，便于离线断言和后续适配。 */
    static QUrlQuery buildControlQuery(Direction direction, bool start,
                                       int channelId = 1, int speed = 4);
    static QString directionName(Direction direction);

signals:
    void ptzReady(bool supported);
    void errorOccurred(const QString &reason);

private slots:
    void handleProbeFinished();
    void handleControlFinished();

private:
    QNetworkRequest buildRequest(const QString &path, const QUrlQuery &query = QUrlQuery()) const;
    QUrl endpoint(const QString &path) const;
    void sendControl(const QUrlQuery &query);
    void clearReadyState();
    /* 将八方向/停止映射到设备 API 的 value 字段。 */
    static QString deviceValue(Direction direction, bool start);

    QNetworkAccessManager *m_manager;
    QNetworkReply *m_probeReply;
    QNetworkReply *m_controlReply;
    QUrl m_webUrl;
    QString m_user;
    QString m_password;
    bool m_ready;
    bool m_moveActive;
    int m_channelId;
    int m_ptzSpeed;
    std::function<void(const QString &, const QString &, const QString &)> m_controlForwarder;
};

#endif // PTZCLIENT_H
