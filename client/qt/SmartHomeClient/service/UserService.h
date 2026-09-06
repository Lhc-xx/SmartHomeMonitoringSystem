#ifndef USERSERVICE_H
#define USERSERVICE_H

#include <QObject>

#include "protocol/ClientProtocol.h"

class TcpClient;

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

signals:
    void registerSuccess();
    void registerFailed(const QString &reason);
    void loginSuccess(quint64 userId);
    void loginFailed(const QString &reason);
    void deviceListReceived(const QList<ClientProtocol::DeviceInfo> &devices);
    void recordListReceived(const QList<ClientProtocol::RecordInfo> &records);
    void requestFailed(const QString &reason);

private slots:
    void onDataReceived(const QByteArray &data);
    void onTcpError(const QString &message);
    void onDisconnected();

private:
    /* 串行请求状态确保新操作不会覆盖正在等待的 requestId 和响应缓冲。 */
    enum class PendingRequest { None, Register, Login, DeviceList, RecordQuery };

    bool beginRequest(PendingRequest type, const QByteArray &packet, quint32 requestId,
                      const QString &actionName);
    bool canStartAuthenticatedRequest(const QString &actionName);
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
};

#endif // USERSERVICE_H
