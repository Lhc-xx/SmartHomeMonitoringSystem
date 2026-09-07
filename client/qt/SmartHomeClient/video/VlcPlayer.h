#ifndef VLCPLAYER_H
#define VLCPLAYER_H

#include <QObject>
#include <QString>

#include <vlc/vlc.h>

/*
 * VlcPlayer 是对 libvlc 的轻量封装，作为可选的 RTSP / 本地文件播放后端。
 *
 * 本类由 player-xia 工程中的 VLCKits 迁移而来（原作者：xia），在保留其
 * 事件驱动播放能力的同时做了如下收敛：
 *   - 修复重复播放时旧 libvlc_media_t 未释放导致的内存泄漏；
 *   - 所有成员方法对未初始化 / 播放失败的 media player 做空指针保护；
 *   - 成员命名统一为 m_ 前缀，与 RtspPlayer 等客户端类保持一致；
 *   - 增加 stateChanged / errorOccurred 信号，供 MonitoringDashboard 接入。
 *
 * 与 RtspPlayer（ffmpeg 子进程 + JPEG 管道，输出 QImage）不同，VlcPlayer
 * 通过 libvlc_media_player_set_hwnd 把画面直接渲染到调用方提供的原生窗口
 * 句柄，无需转码，因此更适合直连摄像头；宿主控件需要开启原生窗口
 * （VideoWidget::setNativeVideoMode）并把自己的 winId() 交给本类。
 *
 * 注意：libvlc 库文件（libvlc.lib / libvlccore.lib 及对应 DLL）未随仓库
 * 提交，需要本机安装 VLC SDK 或完整 VLC 后通过 CMake 的 WITH_VLC=ON 显式
 * 开启编译；运行时还需把 libvlc.dll / libvlccore.dll 及 plugins 目录放到
 * 可执行文件同级。
 */
class VlcPlayer : public QObject
{
    Q_OBJECT

public:
    explicit VlcPlayer(QObject *parent = nullptr);
    ~VlcPlayer() override;

    /* 创建 libvlc 实例、media player 并挂接事件回调；失败返回 false。 */
    bool init();

    /* 播放网络地址（RTSP/HTTP 等），画面渲染到 hwnd 指定的原生窗口。 */
    bool playUrl(const QString &url, void *hwnd);

    /* 播放本地文件路径，画面渲染到 hwnd 指定的原生窗口。 */
    bool playFile(const QString &path, void *hwnd);

    /* 从暂停 / 停止状态恢复播放。 */
    void play();
    void pause();
    void stop();

    void setVolume(int volume);       // 0 ~ 100
    void setPosition(int percent);    // 0 ~ 100

    bool isInitialized() const;

signals:
    /* 播放位置（0~100）变化，对应 libvlc_MediaPlayerPositionChanged。 */
    void positionChanged(int percent);
    /* 当前时间 / 总时长文本，例如 "00:00:12/00:01:30"。 */
    void timeTextChanged(const QString &text);
    /* 音量（0~100）变化。 */
    void volumeChanged(int volume);
    /* 播放状态文本，用于界面状态覆盖层。 */
    void stateChanged(const QString &state);
    /* 初始化或播放过程中的错误描述。 */
    void errorOccurred(const QString &reason);

private:
    static void handleEvent(const libvlc_event_t *event, void *data);
    void updatePosition();
    void updateTime();
    void updateVolume();
    void applyState(libvlc_state_t state);

    libvlc_instance_t *m_instance;
    libvlc_media_t *m_media;
    libvlc_media_player_t *m_mediaPlayer;
    libvlc_event_manager_t *m_eventManager;
    libvlc_time_t m_totalSeconds;
};

#endif // VLCPLAYER_H
