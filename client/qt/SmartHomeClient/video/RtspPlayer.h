#ifndef RTSPPLAYER_H
#define RTSPPLAYER_H

#include <QByteArray>
#include <QImage>
#include <QList>
#include <QObject>
#include <QProcess>
#include <QStringList>

class QTimer;

/*
 * RtspPlayer 负责把一条 RTSP 地址适配为 Qt 可显示的 QImage 帧。
 *
 * 当前 Windows 环境没有 Qt MinGW 32 位可直接链接的 FFmpeg 开发库，
 * 因此播放器以异步 QProcess 管理 ffmpeg.exe。FFmpeg 通过标准输出产生
 * JPEG 图片流，本类只做边界拆分和状态管理，不阻塞界面线程，也不负责绘制。
 */
class RtspPlayer : public QObject
{
    Q_OBJECT

public:
    explicit RtspPlayer(QObject *parent = nullptr);
    ~RtspPlayer() override;

    /* 设置 RTSP 源和 FFmpeg 路径；设置新源会停止当前进程并清理旧缓存。 */
    void setSource(const QString &url, const QString &ffmpegPath = QString());
    void start();
    void stop();
    bool isRunning() const;

    /*
     * 构造可审计的 FFmpeg 参数。把参数集中在这里，既便于测试，也避免
     * 界面层重复拼接命令导致 TCP 传输、JPEG 输出格式不一致。
     */
    static QStringList buildArguments(const QString &url);

    /*
     * 从连续字节流提取完整 JPEG。该纯函数不依赖 QProcess，供单元测试
     * 验证半包、粘包和超大无效数据处理；overflowed 表示缓存已被保护性清空。
     */
    static QList<QByteArray> extractJpegFrames(QByteArray &buffer, bool &overflowed);

signals:
    void frameReady(const QImage &frame);
    void stateChanged(const QString &state);
    void errorOccurred(const QString &reason);

private slots:
    void readStandardOutput();
    void handleProcessError(QProcess::ProcessError error);
    void handleProcessFinished(int exitCode, QProcess::ExitStatus status);
    void handleFrameTimeout();
    void restartAfterBackoff();

private:
    void setState(const QString &state);
    void scheduleReconnect();

    QProcess *m_process;
    QTimer *m_reconnectTimer;
    QTimer *m_frameWatchdog;
    QString m_url;
    QString m_ffmpegPath;
    QByteArray m_frameBuffer;
    int m_backoffSeconds;
    bool m_manualStop;
};

#endif // RTSPPLAYER_H
