#ifndef MONITORINGDASHBOARD_H
#define MONITORINGDASHBOARD_H

#include <QList>
#include <QWidget>

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
    void startPreview();
    void stopPreview();

    QList<VideoWidget *> videoWidgets() const;
    QTreeWidget *deviceTree() const;
    QGroupBox *ptzPanel() const;

signals:
    void eventLogged(const QString &message);
    void requestDeviceList();
    void requestRecordList();

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
    int slotForConfig(const CameraConfig &config, QList<bool> &usedSlots) const;

    QList<CameraConfig> m_cameraConfigs;
    QList<VideoWidget *> m_videoWidgets;
    QList<RtspPlayer *> m_players;
    QList<QPushButton *> m_ptzButtons;
    QList<QPushButton *> m_auxPtzButtons;
    QList<ClientProtocol::DeviceInfo> m_devices;

    QListWidget *m_eventList;
    QLabel *m_statusLabel;
    QTreeWidget *m_deviceTree;
    QGroupBox *m_ptzPanel;
    PtzClient *m_ptzClient;
    QTreeWidgetItem *m_gunItem;
    QTreeWidgetItem *m_domeItem;
};

#endif // MONITORINGDASHBOARD_H
