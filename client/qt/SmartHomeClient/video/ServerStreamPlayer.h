#ifndef SERVERSTREAMPLAYER_H
#define SERVERSTREAMPLAYER_H

#include <QByteArray>
#include <QImage>
#include <QObject>

#include <condition_variable>
#include <deque>
#include <memory>
#include <mutex>
#include <thread>

#include "common/media_reassembler.h"
#include "frame.h"

/*
 * ServerStreamPlayer 负责「服务器转发 → Qt 解码显示」链路：
 *
 *   服务器把一路媒体流以 MediaPacket 帧的形式、和 TLV 响应混在同一条 TCP
 *   连接上发给客户端；UserService 从混合流里切出完整媒体帧后，通过
 *   onMediaFrame() 交给本类。本类用 MediaReassembler 把字节还原成
 *   MediaPacket，再用 IDecoder（真 FFmpegDecoder 或 MockDecoder）解码成
 *   RGBA 帧，最后以 frameReady(QImage) 信号交给界面绘制。
 *
 * 解码在独立工作线程进行，避免阻塞 GUI；与 RtspPlayer（ffmpeg.exe 直连
 * 摄像头 + JPEG 管道）并存，二者二选一。
 */
class ServerStreamPlayer : public QObject
{
    Q_OBJECT

public:
    explicit ServerStreamPlayer(std::unique_ptr<smart_home::client::IDecoder> decoder,
                                QObject *parent = nullptr);
    ~ServerStreamPlayer() override;

    void start();
    void stop();

public slots:
    /* 接收一条完整媒体帧（由 UserService 从混合流切出）。线程安全，可跨线程调用。 */
    void onMediaFrame(const QByteArray &frameBytes);

signals:
    void frameReady(const QImage &frame);
    void stateChanged(const QString &state);
    void errorOccurred(const QString &reason);

private:
    void workerLoop();

    std::unique_ptr<smart_home::client::IDecoder> m_decoder;
    smart_home::common::MediaReassembler m_reassembler;

    std::mutex m_mutex;
    std::condition_variable m_cv;
    std::deque<QByteArray> m_pendingFrames;
    bool m_stopRequested;
    bool m_decoderOpened;
    std::thread m_worker;
};

#endif // SERVERSTREAMPLAYER_H
