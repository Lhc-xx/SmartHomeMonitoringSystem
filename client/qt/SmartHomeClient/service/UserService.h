#ifndef USERSERVICE_H
#define USERSERVICE_H

#include <QObject>

#include "protocol/ClientProtocol.h"

class TcpClient;
class QTimer;

/*
 * UserService 在 UI 与 TcpClient 之间编排认证和 B 数据页请求。
 * token 仅保存于当前进程内存，绝不写日志、文件或界面文本。
 */
class UserService : public QObject
{
    Q_OBJECT

public:
    explicit UserService(TcpClient *tcpClient, QObject *parent = nullptr);

    void registerUser(const QString &username, const QString &password);
    void loginUser(const QString &username, const QString &password);
    void requestDeviceList();
    void requestRecordQuery(quint64 deviceId, const QString &startTime,
                            const QString &endTime);

    /* 请求服务器转发一路流（streamUrl 为空 = Mock 源），/ 停止转发。 */
    void startStream(const QString &streamUrl);
    void stopStream();

    /* 云台控制经服务器转发：cameraUrl 为球机 Web 根地址，direction/move 见协议。 */
    void sendPtzControl(const QString &cameraUrl, const QString &direction, const QString &move);

    /* 服务器转发流已建立后，按选中设备开始/停止服务端录像。 */
    void startRecording(quint64 deviceId);
    void stopRecording();

    /* 设置单次请求的等待超时（毫秒），超时后通过对应失败信号上报。 */
    void setRequestTimeout(int ms);

signals:
    void registerSuccess();
    void registerFailed(const QString &reason);
    void loginSuccess(quint64 userId);
    void loginFailed(const QString &reason);
    /* TCP 仍在异步连接时，通知界面请求已排队而不是误报发送失败。 */
    void requestWaiting(const QString &message);
    void deviceListReceived(const QList<ClientProtocol::DeviceInfo> &devices);
    void recordListReceived(const QList<ClientProtocol::RecordInfo> &records);
    void requestFailed(const QString &reason);

    /* 推流/停流响应成功。 */
    void streamStarted();
    void streamStopped();

    /* 录像开关响应成功；界面据此更新按钮状态。 */
    void recordingStarted();
    void recordingStopped();

    /* 从混合 TCP 流里切出的一条完整媒体帧（原始字节，交给解码器）。 */
    void mediaFrameReceived(const QByteArray &frameBytes);

private slots:
    void onDataReceived(const QByteArray &data);
    void onConnected();
    void onTcpError(const QString &message);
    void onDisconnected();
    void onRequestTimeout();
    void dispatchNextRequest();

private:
    /* 串行请求状态确保新操作不会覆盖正在等待的 requestId 和响应缓冲。 */
    enum class PendingRequest {
        None,
        Register,
        Login,
        DeviceList,
        RecordQuery,
        StreamStart,
        StreamStop,
        RecordStart,
        RecordStop,
        PtzControl
    };

    /*
     * 请求队列只保存已经完成协议编码的字节，避免响应等待期间再次编码或
     * 读取已经变化的登录状态。isPtzStop 用来让松开方向键的 stop 抢在
     * 其它待发操作前面，同时不打断当前正在等待的请求。
     */
    struct QueuedRequest {
        PendingRequest type;
        QByteArray packet;
        quint32 requestId;
        QString actionName;
        bool isPtzStop;
    };

    bool beginRequest(PendingRequest type, const QByteArray &packet, quint32 requestId,
                      const QString &actionName);
    void enqueueRequest(const QueuedRequest &request);
    void scheduleNextRequest();
    bool canStartAuthenticatedRequest(const QString &actionName);
    void completePending();
    void failPending(const QString &reason);
    QString errorCodeMessage(ErrorCode errorCode, const QString &actionName) const;
    quint32 nextRequestId();

    TcpClient *m_tcpClient;
    QByteArray m_receiveBuffer;
    PendingRequest m_pending;
    quint32 m_pendingRequestId;
    quint32 m_nextRequestId;
    quint64 m_userId;
    QByteArray m_token;
    QTimer *m_requestTimer;
    int m_requestTimeoutMs;
    QList<QueuedRequest> m_requestQueue;
    bool m_dispatchScheduled;
};

#endif // USERSERVICE_H
