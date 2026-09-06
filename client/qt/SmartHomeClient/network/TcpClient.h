#ifndef TCPCLIENT_H
#define TCPCLIENT_H

#include <QAbstractSocket>
#include <QByteArray>
#include <QObject>
#include <QString>

class QTcpSocket;

/*
 * TcpClient 类职责：
 *
 * 作为客户端唯一的 TCP 通信封装，负责管理 QTcpSocket 的生命周期、
 * 转发连接/断开/收包事件，并将底层网络错误转换成可直接展示给界面的文本。
 * 业务层只能调用本类公开接口，不能直接操作 QTcpSocket。
 */
class TcpClient : public QObject
{
    Q_OBJECT

public:
    /*
     * 采用 QObject 父对象机制管理 TcpClient，避免主窗口销毁时遗漏释放网络对象。
     */
    explicit TcpClient(QObject *parent = nullptr);

    /*
     * 异步发起 TCP 连接。
     *
     * QTcpSocket 的连接过程由 Qt 事件循环驱动，调用后通过 connected() 或
     * errorOccurred() 通知结果，避免阻塞界面线程。
     */
    void connectServer(const QString &ip, quint16 port);

    /*
     * 将完整的业务协议字节流写入 Qt 的发送缓冲区。
     *
     * 本方法不拼接协议，也不等待网络写完成；协议层和业务层保持独立，
     * 发送前的连接状态与写入失败会通过 errorOccurred() 上报。
     */
    void sendData(const QByteArray &data);

    /*
     * 请求正常断开当前连接。
     *
     * 使用 disconnectFromHost() 可让 Qt 先处理已经写入缓冲区的数据，
     * 后续断开结果仍统一通过 disconnected() 或 errorOccurred() 通知。
     */
    void disconnectServer();

signals:
    /* TCP 三次握手完成后发出，供后续连接管理界面使用。 */
    void connected();

    /* 套接字关闭后发出，供业务层清理等待中的请求。 */
    void disconnected();

    /* 收到的原始 TCP 字节流，不在网络层假设业务消息边界。 */
    void dataReceived(const QByteArray &data);

    /*
     * 将连接、发送和异常断开错误统一转换为中文说明，
     * 使界面无需依赖 QAbstractSocket 的底层错误枚举。
     */
    void errorOccurred(const QString &message);

private slots:
    /* 读取当前可用字节并交给上层协议处理，保留 TCP 流式传输特性。 */
    void readData();

    /* 转发 QTcpSocket 的连接成功事件，隔离底层套接字对象。 */
    void onConnected();

    /* 将所有底层网络异常集中转换为界面可展示的错误文本。 */
    void onSocketError(QAbstractSocket::SocketError socketError);

private:
    /* 真正执行 TCP 通信的 Qt 套接字，只由 TcpClient 管理。 */
    QTcpSocket *m_socket;
};

#endif // TCPCLIENT_H
