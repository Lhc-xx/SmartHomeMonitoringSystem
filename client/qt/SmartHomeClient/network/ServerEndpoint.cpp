#include "ServerEndpoint.h"

#include <QByteArray>
#include <QtGlobal>

namespace {

const quint16 kDefaultServerPort = 7777;

/* 读取非空主机名；空字符串等同于未配置，继续使用本机默认地址。 */
QString configuredHost()
{
    const QString value = qEnvironmentVariable("SMARTHOME_SERVER_IP").trimmed();
    return value.isEmpty() ? QStringLiteral("127.0.0.1") : value;
}

/* 严格解析端口，避免 QByteArray::toUShort 对越界/非数字输入的静默误用。 */
quint16 configuredPort()
{
    const QByteArray value = qgetenv("SMARTHOME_SERVER_PORT").trimmed();
    if (value.isEmpty()) {
        return kDefaultServerPort;
    }

    bool ok = false;
    const uint parsed = value.toUInt(&ok, 10);
    if (!ok || parsed == 0 || parsed > 65535U) {
        return kDefaultServerPort;
    }
    return static_cast<quint16>(parsed);
}

} // namespace

ServerEndpoint resolveServerEndpoint()
{
    /* 集中在一个小函数中生成值对象，MainWindow 不再携带部署地址解析细节。 */
    ServerEndpoint endpoint;
    endpoint.host = configuredHost();
    endpoint.port = configuredPort();
    return endpoint;
}
