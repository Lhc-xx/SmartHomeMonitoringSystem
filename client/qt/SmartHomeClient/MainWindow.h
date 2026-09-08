#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>

#include "protocol/ClientProtocol.h"
#include "video/CameraConfig.h"

class DeviceModel;
class FilePlaybackPlayer;
class QCheckBox;
class QDateTimeEdit;
class QListView;
class QLabel;
class LoginWidget;
class MonitoringDashboard;
class QPushButton;
class RecordModel;
class ServerStreamPlayer;
class TcpClient;
class UserService;
class QWidget;

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

/*
 * MainWindow 负责对象组装和登录后的 B 数据页切换。
 * 数据页只展示设备及录像元数据，不创建播放控件，也不触及 FFmpeg。
 */
class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

    /*
     * 按“显式环境变量 → 可执行文件目录 → 项目配置目录”的顺序定位摄像头配置。
     * 抽成纯函数后可以在不启动摄像头的情况下测试 Qt Creator 不同构建目录。
     */
    static QString resolveCameraConfigPath(const QString &configuredPath,
                                           const QString &applicationDir);

private slots:
    void showDataPage(quint64 userId);
    void showDashboardPage();
    void showMetadataPage();
    void requestDevices();
    void requestRecords();
    void requestRecordsForDevice(quint64 deviceId);
    void updateDevices(const QList<ClientProtocol::DeviceInfo> &devices);
    void updateRecords(const QList<ClientProtocol::RecordInfo> &records);
    void showRequestError(const QString &reason);
    void handlePlaybackRequest(quint64 deviceId);
    void playSelectedRecord();
    void handleRecordStart(quint64 deviceId);
    void handleRecordStop();
    void handleStreamStarted();
    void handleStreamStopped();

private:
    QWidget *createDataPage();
    void setStatusMessage(const QString &message);
    void playRecordAt(int row);
    void selectDeviceInDataPage(quint64 deviceId);
    QString configuredFfmpegPath() const;

    /*
     * 把服务端返回的相对录像路径映射到客户端已挂载的录像根目录。
     * root 为空时保持原路径，避免伪造一个不存在的远程文件传输能力。
     */
public:
    static QString resolveRecordFilePath(const QString &recordPath,
                                         const QString &mountedRecordRoot);

private:

    Ui::MainWindow *ui;
    TcpClient *m_tcpClient;
    UserService *m_userService;
    LoginWidget *m_loginWidget;
    MonitoringDashboard *m_dashboard;
    ServerStreamPlayer *m_serverStreamPlayer;
    FilePlaybackPlayer *m_playbackPlayer;
    QList<ClientProtocol::RecordInfo> m_records;
    bool m_pendingPlayback;
    bool m_serverStreamActive;
    bool m_serverStreamStarting;
    bool m_recordStartPending;
    quint64 m_pendingRecordDeviceId;
    QList<CameraConfig> m_cameraConfigs;
    QWidget *m_dataPage;
    DeviceModel *m_deviceModel;
    RecordModel *m_recordModel;
    QListView *m_deviceList;
    QListView *m_recordList;
    QLabel *m_dataStatus;
    QPushButton *m_recordQueryButton;
    QPushButton *m_playSelectedButton;
    QPushButton *m_backToPreviewButton;
    QCheckBox *m_allTimeCheckBox;
    QDateTimeEdit *m_recordStartEdit;
    QDateTimeEdit *m_recordEndEdit;
};

#endif // MAINWINDOW_H
