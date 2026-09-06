#include "TcpClient.h"

#include <QTcpSocket>

/*
 * 创建 QTcpSocket 并在构造阶段完成所有信号连接。
 *
 * 将连接关系集中在这里，可确保后续业务代码不会重复订阅底层套接字信号。
 */
TcpClient::TcpClient(QObject *parent)
    : QObject(parent)
    , m_socket(new QTcpSocket(this))
{
    connect(m_socket, &QTcpSocket::readyRead, this, &TcpClient::readData);
    connect(m_socket, &QTcpSocket::connected, this, &TcpClient::onConnected);
    connect(m_socket, &QTcpSocket::disconnected, this, &TcpClient::disconnected);

    /*
     * Qt 5.14 使用 QAbstractSocket::error 信号。
     * 显式转换信号类型可避免同名成员带来的重载歧义，并保持 Qt5 兼容性。
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
    /*
     * 在调用 QTcpSocket 前校验明显无效的目标，
     * 这样界面能立刻获得可理解的错误，而不是等待一次无意义的底层调用。
     */
    if (ip.trimmed().isEmpty() || port == 0) {
        emit errorOccurred(QStringLiteral("连接失败：服务器地址或端口无效。"));
        return;
    }

    /* QTcpSocket 的 connectToHost 是异步操作，不会阻塞 GUI 线程。 */
    m_socket->connectToHost(ip.trimmed(), port);
}

void TcpClient::sendData(const QByteArray &data)
{
    /*
     * 只有连接建立后才允许排队发送。
     * 该判断把“未连接就发送”的错误从协议层和业务层中隔离出来。
     */
    if (m_socket->state() != QAbstractSocket::ConnectedState) {
        emit errorOccurred(QStringLiteral("发送失败：TCP 连接尚未建立。"));
        return;
    }

    if (data.isEmpty()) {
        emit errorOccurred(QStringLiteral("发送失败：协议数据为空。"));
        return;
    }

    /*
     * write() 返回已进入 Qt 发送缓冲区的字节数。
     * 返回负数或不足完整报文都不能视为发送成功，统一报告给上层。
     */
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
    /*
     * 无连接时不调用底层断开接口，直接给出明确状态，
     * 防止界面把重复断开误判为已完成一次有效网络操作。
     */
    if (m_socket->state() == QAbstractSocket::UnconnectedState) {
        emit errorOccurred(QStringLiteral("断开失败：当前没有活动的 TCP 连接。"));
        return;
    }

    m_socket->disconnectFromHost();
}

void TcpClient::readData()
{
    /*
     * TCP 是字节流协议，readAll() 得到的内容可能是半包或多个包。
     * 因此这里只转发原始数据，由 ClientProtocol/UserService 负责缓冲和拆包。
     */
    const QByteArray data = m_socket->readAll();
    if (!data.isEmpty()) {
        emit dataReceived(data);
    }
}

void TcpClient::onConnected()
{
    /* 对上层隐藏 QTcpSocket，保持网络层唯一出口。 */
    emit connected();
}

void TcpClient::onSocketError(QAbstractSocket::SocketError socketError)
{
    Q_UNUSED(socketError)

    /*
     * QAbstractSocket 已保存详细错误文本；将其加上统一前缀后交给 UI，
     * 可以覆盖连接失败、发送失败以及异常断开的情况。
     */
    const QString detail = m_socket->errorString().isEmpty()
        ? QStringLiteral("发生未知网络错误。")
        : m_socket->errorString();
    emit errorOccurred(QStringLiteral("TCP 通信错误：%1").arg(detail));
}
