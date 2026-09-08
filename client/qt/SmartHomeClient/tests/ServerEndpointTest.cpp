#include <QtTest>

#include "network/ServerEndpoint.h"

/*
 * ServerEndpointTest 类职责：
 *
 * 验证 Qt 客户端服务器地址解析的默认值、环境变量覆盖和非法端口回退。
 * 测试只读取/设置当前进程环境变量，不创建 socket，也不会访问远程服务器。
 */
class ServerEndpointTest : public QObject
{
    Q_OBJECT

private slots:
    /* 没有部署配置时必须使用安全的本机默认地址。 */
    void usesLoopbackByDefault();

    /* 部署脚本可以通过环境变量指定 ECS 或局域网服务器。 */
    void acceptsEnvironmentOverrides();

    /* 端口越界、非数字和空值都不能传入 QTcpSocket。 */
    void rejectsInvalidPorts();
};

void ServerEndpointTest::usesLoopbackByDefault()
{
    qunsetenv("SMARTHOME_SERVER_IP");
    qunsetenv("SMARTHOME_SERVER_PORT");

    const ServerEndpoint endpoint = resolveServerEndpoint();
    QCOMPARE(endpoint.host, QStringLiteral("127.0.0.1"));
    QCOMPARE(endpoint.port, static_cast<quint16>(7777));
}

void ServerEndpointTest::acceptsEnvironmentOverrides()
{
    qputenv("SMARTHOME_SERVER_IP", QByteArray("192.0.2.40"));
    qputenv("SMARTHOME_SERVER_PORT", QByteArray("8888"));

    const ServerEndpoint endpoint = resolveServerEndpoint();
    QCOMPARE(endpoint.host, QStringLiteral("192.0.2.40"));
    QCOMPARE(endpoint.port, static_cast<quint16>(8888));
}

void ServerEndpointTest::rejectsInvalidPorts()
{
    qputenv("SMARTHOME_SERVER_IP", QByteArray("192.0.2.40"));

    qputenv("SMARTHOME_SERVER_PORT", QByteArray("0"));
    QCOMPARE(resolveServerEndpoint().port, static_cast<quint16>(7777));

    qputenv("SMARTHOME_SERVER_PORT", QByteArray("65536"));
    QCOMPARE(resolveServerEndpoint().port, static_cast<quint16>(7777));

    qputenv("SMARTHOME_SERVER_PORT", QByteArray("not-a-port"));
    QCOMPARE(resolveServerEndpoint().port, static_cast<quint16>(7777));
}

QTEST_APPLESS_MAIN(ServerEndpointTest)

#include "ServerEndpointTest.moc"
