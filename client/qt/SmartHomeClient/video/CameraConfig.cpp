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
