#ifndef FILEPLAYBACKPLAYER_H
#define FILEPLAYBACKPLAYER_H

#include <QByteArray>
#include <QImage>
#include <QObject>
#include <QProcess>
#include <QString>
#include <QStringList>

/*
 * FilePlaybackPlayer 负责「录像回放」：用 ffmpeg.exe 把本地录像文件（TS/MP4）
 * 解码成 JPEG 帧流，与 RtspPlayer（直连摄像头）并存。复用 RtspPlayer 的
 * JPEG 拆包逻辑，只做本地文件播放，不带 RTSP 参数。
 */
class FilePlaybackPlayer : public QObject
{
    Q_OBJECT

public:
    explicit FilePlaybackPlayer(QObject *parent = nullptr);
    ~FilePlaybackPlayer() override;

    /* 播放本地录像文件路径；ffmpegPath 为空时用系统 PATH 里的 ffmpeg。 */
    void play(const QString &path, const QString &ffmpegPath = QString());
    void stop();
    bool isRunning() const;

    /* 构造可审计的 ffmpeg 参数（本地文件，无 -rtsp_transport）。 */
    static QStringList buildArguments(const QString &path);

signals:
    void frameReady(const QImage &frame);
    void stateChanged(const QString &state);
    void errorOccurred(const QString &reason);

private slots:
    void readStandardOutput();
    void handleProcessError(QProcess::ProcessError error);
    void handleProcessFinished(int exitCode, QProcess::ExitStatus status);

private:
    QProcess *m_process;
    QByteArray m_frameBuffer;
    bool m_manualStop;
};

#endif // FILEPLAYBACKPLAYER_H
