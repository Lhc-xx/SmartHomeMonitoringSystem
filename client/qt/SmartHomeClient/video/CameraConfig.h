#ifndef CAMERACONFIG_H
#define CAMERACONFIG_H

#include <QList>
#include <QString>

/*
 * CameraConfig 负责描述一台摄像头的本地连接信息。
 *
 * 该结构只在 Qt 客户端内存中传递配置；真实账号、密码和 RTSP 地址
 * 来自被 .gitignore 忽略的本地配置文件，绝不会写入协议包、测试或日志。
 */
struct CameraConfig
{
    QString name;       // 界面显示名称，例如“枪机”或“球机”。
    QString type;       // 设备类型：gun 表示枪机，dome 表示球机。
    QString rtspUrl;    // 摄像头 RTSP 地址，供 RtspPlayer 启动 FFmpeg。
    QString webUrl;     // 摄像头网页服务根地址，供 PtzClient 使用。
    QString user;       // 摄像头本地账号，仅保存在客户端进程内存。
    QString password;   // 摄像头本地密码，仅保存在客户端进程内存。
    QString ffmpegPath; // FFmpeg 可执行文件路径；为空时使用系统 PATH。
    bool enabled = false;
};

/*
 * 从 INI 文件读取摄像头配置。
 *
 * 设计为独立函数是为了让界面、播放器和测试共享同一份校验逻辑，
 * 避免各模块自行解析导致地址格式或 enabled 语义不一致。
 */
QList<CameraConfig> loadCameraConfigs(const QString &path, QString *errorMessage = nullptr);

#endif // CAMERACONFIG_H
