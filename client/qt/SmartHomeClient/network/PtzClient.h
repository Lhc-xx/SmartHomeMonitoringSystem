#ifndef PTZCLIENT_H
#define PTZCLIENT_H

#include <QNetworkRequest>
#include <QObject>
#include <QUrl>
#include <QUrlQuery>

class QNetworkAccessManager;
class QNetworkReply;

/*
 * PtzClient 负责把 Qt 工作台的云台操作适配为摄像头网页 API 请求。
 *
 * 它只访问摄像头 Web 服务，不接触项目 TCP/TLV 协议。程序必须先完成
 * /api/ptz/baseConf 的只读探测，方向按钮在收到成功响应前保持禁用；
 * 真实移动只由用户按下/释放按钮触发，停止请求在释放、失焦和析构时补发。
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

    /* channelId 与设备网页端的逻辑通道一致，RTSP chn=0 对应默认值 1。 */
    void setCamera(const QUrl &webUrl, const QString &user, const QString &password,
                   int channelId = 1);
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
    /* 将方向转换为摄像头网页 API 的 value 字段，停止统一使用 s。 */
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
};

#endif // PTZCLIENT_H
