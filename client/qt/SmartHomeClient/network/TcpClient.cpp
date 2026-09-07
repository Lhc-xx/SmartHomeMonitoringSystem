#include "TcpClient.h"

#include <QTimer>
#include <QTcpSocket>

TcpClient::TcpClient(QObject *parent)
    : QObject(parent)
    , m_socket(new QTcpSocket(this))
    , m_reconnectTimer(new QTimer(this))
    , m_port(0)
    , m_autoReconnect(true)
    , m_manualDisconnect(false)
    , m_reconnectPending(false)
    , m_reconnectAttempt(0)
    , m_reconnectDelayMs(1000)
    , m_reconnectMaxAttempts(-1)
{
    m_reconnectTimer->setSingleShot(true);

    connect(m_socket, &QTcpSocket::readyRead, this, &TcpClient::readData);
    connect(m_socket, &QTcpSocket::connected, this, &TcpClient::onConnected);
    connect(m_socket, &QTcpSocket::disconnected, this, &TcpClient::onDisconnected);
    connect(m_reconnectTimer, &QTimer::timeout, this, &TcpClient::attemptReconnect);

    /*
     * Qt 5.14 使用 QAbstractSocket::error 信号，显式转换信号类型可避免
     * 同名成员带来的重载歧义，并保持 Qt5 兼容性。
     */
    connect(
        m_socket,
        static_cast<void (QAbstractSocket::*)(QAbstractSocket::SocketError)>(
            &QAbstractSocket::error),
        this,
        &TcpClient::onSocketError);
}

void TcpClient::connectServer(const QString &ip, quint16 port)
{
    if (ip.trimmed().isEmpty() || port == 0) {
        emit errorOccurred(QStringLiteral("连接失败：服务器地址或端口无效。"));
        return;
    }

    m_ip = ip.trimmed();
    m_port = port;
    m_manualDisconnect = false;
    resetReconnectState();

    /* 若已连接或正在连接，先静默中断旧连接，避免这次切换被误判为一次断线重连。 */
    if (m_socket->state() != QAbstractSocket::UnconnectedState) {
        m_manualDisconnect = true;
        m_socket->abort();
        m_manualDisconnect = false;
    }

    m_socket->connectToHost(m_ip, m_port);
}

void TcpClient::sendData(const QByteArray &data)
{
    if (m_socket->state() != QAbstractSocket::ConnectedState) {
        emit errorOccurred(QStringLiteral("发送失败：TCP 连接尚未建立。"));
        return;
    }

    if (data.isEmpty()) {
        emit errorOccurred(QStringLiteral("发送失败：协议数据为空。"));
        return;
    }

    const qint64 written = m_socket->write(data);
    if (written != data.size()) {
        const QString detail = m_socket->errorString().isEmpty()
            ? QStringLiteral("套接字未接受完整数据。")
            : m_socket->errorString();
        emit errorOccurred(QStringLiteral("发送失败：%1").arg(detail));
    }
}

void TcpClient::disconnectServer()
{
    /* 主动断开：关闭自动重连，随后触发的 disconnected 不再进入重连流程。 */
    m_manualDisconnect = true;
    m_reconnectTimer->stop();
    m_reconnectPending = false;

    if (m_socket->state() == QAbstractSocket::UnconnectedState) {
        emit errorOccurred(QStringLiteral("断开失败：当前没有活动的 TCP 连接。"));
        return;
    }

    m_socket->disconnectFromHost();
}

void TcpClient::setAutoReconnect(bool enabled)
{
    m_autoReconnect = enabled;
    if (!enabled) {
        m_reconnectTimer->stop();
        m_reconnectPending = false;
    }
}

void TcpClient::setReconnectDelay(int ms)
{
    m_reconnectDelayMs = ms < 0 ? 0 : ms;
}

void TcpClient::setReconnectMaxAttempts(int maxAttempts)
{
    m_reconnectMaxAttempts = maxAttempts;
}

void TcpClient::readData()
{
    const QByteArray data = m_socket->readAll();
    if (!data.isEmpty()) {
        emit dataReceived(data);
    }
}

void TcpClient::onConnected()
{
    resetReconnectState();
    emit connected();
}

void TcpClient::onDisconnected()
{
    emit disconnected();
    if (m_manualDisconnect) {
        return;
    }
    if (m_autoReconnect) {
        scheduleReconnect();
    }
}

void TcpClient::onSocketError(QAbstractSocket::SocketError socketError)
{
    Q_UNUSED(socketError)

    const QString detail = m_socket->errorString().isEmpty()
        ? QStringLiteral("发生未知网络错误。")
        : m_socket->errorString();
    emit errorOccurred(QStringLiteral("TCP 通信错误：%1").arg(detail));

    /*
     * 仅当连接尚未建立（连接失败）时在此重试；已建立的连接断开走
     * onDisconnected，避免同一次断开被计数两次。
     */
    if (m_autoReconnect && !m_manualDisconnect
        && m_socket->state() == QAbstractSocket::UnconnectedState) {
        scheduleReconnect();
    }
}

void TcpClient::scheduleReconnect()
{
    if (m_reconnectPending) {
        return;
    }
    if (m_reconnectMaxAttempts >= 0 && m_reconnectAttempt >= m_reconnectMaxAttempts) {
        emit reconnectFailed();
        return;
    }

    ++m_reconnectAttempt;
    m_reconnectPending = true;

    int delay = m_reconnectDelayMs;
    for (int i = 1; i < m_reconnectAttempt; ++i) {
        delay *= 2;
        if (delay > 30000) {
            delay = 30000;
            break;
        }
    }

    emit reconnecting(m_reconnectAttempt);
    m_reconnectTimer->start(delay);
}

void TcpClient::attemptReconnect()
{
    m_reconnectPending = false;
    if (m_ip.isEmpty() || m_port == 0 || m_manualDisconnect) {
        return;
    }
    m_socket->connectToHost(m_ip, m_port);
}

void TcpClient::resetReconnectState()
{
    m_reconnectAttempt = 0;
    m_reconnectPending = false;
    m_reconnectTimer->stop();
}
