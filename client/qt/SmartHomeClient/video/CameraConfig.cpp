#include "CameraConfig.h"

#include <QFileInfo>
#include <QSettings>
#include <QUrl>

namespace {

void setError(QString *errorMessage, const QString &message)
{
    if (errorMessage != nullptr) {
        *errorMessage = message;
    }
}

bool parseBool(const QString &value, bool defaultValue)
{
    if (value.isEmpty()) {
        return defaultValue;
    }

    const QString normalized = value.trimmed().toLower();
    if (normalized == QStringLiteral("1")
        || normalized == QStringLiteral("true")
        || normalized == QStringLiteral("yes")) {
        return true;
    }
    if (normalized == QStringLiteral("0")
        || normalized == QStringLiteral("false")
        || normalized == QStringLiteral("no")) {
        return false;
    }
    return defaultValue;
}

/*
 * 只接受两种明确传输模式。配置错误必须返回 false，不能随意回退：
 * 一旦误选路径，云台请求可能被发往没有摄像头路由的服务器，现场表现为“按键无反应”。
 */
bool parsePtzTransport(const QString &value, CameraConfig::PtzTransport &transport)
{
    const QString normalized = value.trimmed().toLower();
    if (normalized.isEmpty() || normalized == QStringLiteral("direct")) {
        transport = CameraConfig::PtzTransport::Direct;
        return true;
    }
    if (normalized == QStringLiteral("server")) {
        transport = CameraConfig::PtzTransport::Server;
        return true;
    }
    return false;
}

bool isRtspUrl(const QString &value)
{
    const QUrl url(value);
    return url.isValid() && url.scheme().compare(QStringLiteral("rtsp"), Qt::CaseInsensitive) == 0
        && !url.host().isEmpty();
}

}

QList<CameraConfig> loadCameraConfigs(const QString &path, QString *errorMessage)
{
    if (errorMessage != nullptr) {
        errorMessage->clear();
    }

    if (path.trimmed().isEmpty() || !QFileInfo::exists(path)) {
        setError(errorMessage, QStringLiteral("摄像头配置文件不存在"));
        return QList<CameraConfig>();
    }

    QSettings settings(path, QSettings::IniFormat);
    const QStringList groups = settings.childGroups();
    if (groups.isEmpty()) {
        setError(errorMessage, QStringLiteral("摄像头配置文件没有有效分组"));
        return QList<CameraConfig>();
    }

    QList<CameraConfig> configs;
    for (const QString &group : groups) {
        settings.beginGroup(group);

        CameraConfig config;
        config.name = settings.value(QStringLiteral("name"), group).toString().trimmed();
        config.type = settings.value(QStringLiteral("type"), group).toString().trimmed().toLower();
        config.rtspUrl = settings.value(QStringLiteral("rtspUrl")).toString().trimmed();
        config.webUrl = settings.value(QStringLiteral("webUrl")).toString().trimmed();
        config.user = settings.value(QStringLiteral("user")).toString();
        config.password = settings.value(QStringLiteral("password")).toString();
        config.ffmpegPath = settings.value(QStringLiteral("ffmpegPath")).toString().trimmed();
        if (!parsePtzTransport(settings.value(QStringLiteral("ptzTransport")).toString(),
                               config.ptzTransport)) {
            settings.endGroup();
            setError(errorMessage, QStringLiteral("摄像头分组“%1”的 ptzTransport 必须是 direct 或 server")
                     .arg(group));
            return QList<CameraConfig>();
        }
        config.enabled = parseBool(settings.value(QStringLiteral("enabled")).toString(), false);
        settings.endGroup();

        if (config.name.isEmpty() || config.type.isEmpty() || !isRtspUrl(config.rtspUrl)) {
            setError(errorMessage, QStringLiteral("摄像头分组“%1”的名称、类型或 RTSP 地址无效").arg(group));
            return QList<CameraConfig>();
        }

        if (!config.webUrl.isEmpty() && !QUrl(config.webUrl).isValid()) {
            setError(errorMessage, QStringLiteral("摄像头分组“%1”的 Web 地址无效").arg(group));
            return QList<CameraConfig>();
        }

        configs.append(config);
    }

    return configs;
}
