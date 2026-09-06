#ifndef TCPCLIENT_H
#define TCPCLIENT_H

#include <QAbstractSocket>
#include <QByteArray>
#include <QObject>
#include <QString>

class QTcpSocket;
class QTimer;

/*
 * TcpClient 类职责：
 *
 * 作为客户端唯一的 TCP 通信封装，负责管理 QTcpSocket 的生命周期、
 * 转发连接/断开/收包事件，并将底层网络错误转换成可直接展示给界面的文本。
 * 业务层只能调用本类公开接口，不能直接操作 QTcpSocket。
 *
 * 断线重连：默认开启。连接意外断开或连接失败时按指数退避自动重试，
 * 通过 setReconnectDelay / setReconnectMaxAttempts 调整节奏，重连过程经
 * reconnecting() / reconnectFailed() 信号上报；主动 disconnectServer() 不会触发重连。
 */
class TcpClient : public QObject
{
    Q_OBJECT

public:
    explicit TcpClient(QObject *parent = nullptr);

    void connectServer(const QString &ip, quint16 port);
    void sendData(const QByteArray &data);
    void disconnectServer();

    void setAutoReconnect(bool enabled);
    void setReconnectDelay(int ms);
    void setReconnectMaxAttempts(int maxAttempts);

signals:
    /* TCP 三次握手完成后发出，供后续连接管理界面使用。 */
    void connected();

    /* 套接字关闭后发出，供业务层清理等待中的请求。 */
    void disconnected();

    /* 收到的原始 TCP 字节流，不在网络层假设业务消息边界。 */
    void dataReceived(const QByteArray &data);

    /* 连接、发送和异常断开错误统一转换为中文说明，供界面直接展示。 */
    void errorOccurred(const QString &message);

    /* 每次计划一次重连时发出，参数为当前重连序号（从 1 开始）。 */
    void reconnecting(int attempt);

    /* 达到最大重连次数仍失败时发出，此后不再自动重连。 */
    void reconnectFailed();

private slots:
    void readData();
    void onConnected();
    void onDisconnected();
    void onSocketError(QAbstractSocket::SocketError socketError);
    void attemptReconnect();

private:
    void scheduleReconnect();
    void resetReconnectState();

    QTcpSocket *m_socket;
    QTimer *m_reconnectTimer;
    QString m_ip;
    quint16 m_port;
    bool m_autoReconnect;
    bool m_manualDisconnect;
    bool m_reconnectPending;
    int m_reconnectAttempt;
    int m_reconnectDelayMs;
    int m_reconnectMaxAttempts;
};

#endif // TCPCLIENT_H
