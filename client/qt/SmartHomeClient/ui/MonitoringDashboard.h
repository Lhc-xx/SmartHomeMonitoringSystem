#ifndef MONITORINGDASHBOARD_H
#define MONITORINGDASHBOARD_H

#include <QList>
#include <QWidget>

#include <functional>

#include "video/CameraConfig.h"
#include "network/PtzClient.h"
#include "protocol/ClientProtocol.h"

class QListWidget;
class QLabel;
class QTreeWidget;
class QTreeWidgetItem;
class QGroupBox;
class QPushButton;
class VideoWidget;
class RtspPlayer;
#ifdef SMART_HOME_WITH_VLC
class VlcPlayer;
#endif

/*
 * MonitoringDashboard 负责登录成功后的监控工作台布局和组件装配。
 *
 * 中央四宫格只接收 RtspPlayer 的画面信号；右侧设备树负责选择枪机/球机，
 * 并把球机选择交给 PtzClient 探测能力。设备/录像协议仍由 MainWindow 和
 * UserService 管理，本类不直接操作 TcpClient，也不复制协议解析逻辑。
 */
class MonitoringDashboard : public QWidget
{
    Q_OBJECT

public:
    explicit MonitoringDashboard(QWidget *parent = nullptr);
    ~MonitoringDashboard() override;

    void setCameraConfigs(const QList<CameraConfig> &configs);
    void setDevices(const QList<ClientProtocol::DeviceInfo> &devices);
    /* 让 MainWindow 的网络/业务结果直接显示在当前可见工作台。 */
    void setStatusMessage(const QString &message);
    /* 将网络失败或业务结果追加到左侧事件面板，避免错误只出现在不可见页面。 */
    void logEvent(const QString &message);
    /* 返回当前本地摄像头对应的 RTSP 地址，仅供开始服务端录像时建立流会话。 */
    QString selectedCameraRtspUrl() const;
    /* 根据服务器响应切换录像按钮，避免请求尚未成功时误显示“停止录像”。 */
    void setRecordingActive(bool active);
    /* 注入云台转发回调（参数：cameraUrl, direction, move），用于经服务器转发。 */
    void setControlForwarder(const std::function<void(const QString &, const QString &, const QString &)> &forwarder);
    void startPreview();
    void stopPreview();

    QList<VideoWidget *> videoWidgets() const;
    QTreeWidget *deviceTree() const;
    QGroupBox *ptzPanel() const;

signals:
    void eventLogged(const QString &message);
    /* 从元数据页返回工作台时复用同一条导航信号。 */
    void requestPreview();
    void requestDeviceList();
    /* 工作台查询必须携带右侧服务端设备树当前选中的设备 ID。 */
    void requestRecordList(quint64 deviceId);
    /* 回放所选设备最近录像。 */
    void requestPlayback(quint64 deviceId);
    /* 录像控制只携带当前服务端设备 ID，不允许使用本地树节点的序号代替。 */
    void requestRecordStart(quint64 deviceId);
    void requestRecordStop();

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private slots:
    void handleDeviceSelection();
    void handlePtzReady(bool supported);
    void handlePtzError(const QString &reason);

private:
    void buildUi();
    void buildPtzControls();
    void appendEvent(const QString &message);
    void applySelectedCamera(QTreeWidgetItem *item);
    void setPtzButtonsEnabled(bool enabled);
    quint64 selectedServerDeviceId() const;
    /* 按本地枪机/球机类型优先寻找对应服务端设备，找不到时使用首个设备。 */
    quint64 fallbackServerDeviceId(const QString &localType) const;
    int slotForConfig(const CameraConfig &config, QList<bool> &usedSlots) const;

    QList<CameraConfig> m_cameraConfigs;
    QList<VideoWidget *> m_videoWidgets;
    QList<RtspPlayer *> m_players;
#ifdef SMART_HOME_WITH_VLC
    QList<VlcPlayer *> m_vlcPlayers;
#endif
    QList<QPushButton *> m_ptzButtons;
    QList<QPushButton *> m_auxPtzButtons;
    QList<ClientProtocol::DeviceInfo> m_devices;
    /*
     * 保存 MainWindow 提供的服务器转发入口，但只在所选球机显式配置为
     * Server 时注入 PtzClient；Direct 模式必须保持空回调并走本机 HTTP。
     */
    std::function<void(const QString &, const QString &, const QString &)> m_controlForwarder;

    QListWidget *m_eventList;
    QLabel *m_statusLabel;
    QTreeWidget *m_deviceTree;
    QGroupBox *m_ptzPanel;
    QPushButton *m_recordControlButton;
    bool m_recordingActive;
    PtzClient *m_ptzClient;
    QTreeWidgetItem *m_gunItem;
    QTreeWidgetItem *m_domeItem;
};

#endif // MONITORINGDASHBOARD_H
