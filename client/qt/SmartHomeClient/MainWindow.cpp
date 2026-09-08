#include "MainWindow.h"

#include <QCheckBox>
#include <QCoreApplication>
#include <QDateTime>
#include <QDateTimeEdit>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QListView>
#include <QPushButton>
#include <QSplitter>
#include <QVBoxLayout>

#include "model/DeviceModel.h"
#include "model/RecordModel.h"
#include "network/ServerEndpoint.h"
#include "network/TcpClient.h"
#include "service/UserService.h"
#include "ui/LoginWidget.h"
#include "ui/MonitoringDashboard.h"
#include "ui_MainWindow.h"
#include "video/CameraConfig.h"
#include "video/FilePlaybackPlayer.h"
#include "video/ServerStreamPlayer.h"
#include "video/VideoWidget.h"

#include <memory>

#ifdef SMART_HOME_WITH_FFMPEG
#include "ffmpeg_decoder.h"
#else
#include "mock_decoder.h"
#endif

namespace {

std::unique_ptr<smart_home::client::IDecoder> makeStreamDecoder()
{
#ifdef SMART_HOME_WITH_FFMPEG
    return std::unique_ptr<smart_home::client::IDecoder>(new smart_home::client::FFmpegDecoder());
#else
    return std::unique_ptr<smart_home::client::IDecoder>(new smart_home::client::MockDecoder());
#endif
}

} // namespace

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , m_tcpClient(nullptr)
    , m_userService(nullptr)
    , m_loginWidget(nullptr)
    , m_dashboard(nullptr)
    , m_serverStreamPlayer(nullptr)
    , m_playbackPlayer(nullptr)
    , m_pendingPlayback(false)
    , m_dataPage(nullptr)
    , m_deviceModel(nullptr)
    , m_recordModel(nullptr)
    , m_deviceList(nullptr)
    , m_dataStatus(nullptr)
    , m_recordQueryButton(nullptr)
    , m_allTimeCheckBox(nullptr)
    , m_recordStartEdit(nullptr)
    , m_recordEndEdit(nullptr)
{
    /* 保留既有 Designer 主窗口外壳，再在中央区域切换登录页和数据页。 */
    ui->setupUi(this);
    /*
     * MainWindow.ui 只保留窗口外壳；中央页面由本类在运行时安装。
     * 因此不会遗留 Designer 自动生成的空白 centralwidget、菜单栏或状态栏。
     */
    setWindowTitle(QStringLiteral("Smart Home Monitoring System"));
    setMinimumSize(900, 620);
    resize(980, 700);

    /*
     * 数据页沿用登录页的绿色品牌色，但以浅色内容区承载列表，
     * 让用户在登录成功后能快速区分操作栏、设备列表和录像列表。
     * 这里只设置 QWidget 外观，不改变数据请求、模型或信号连接。
     */
    setStyleSheet(QStringLiteral(R"(
        QMainWindow {
            background: #ffffff;
            font-family: "Segoe UI";
        }
        QWidget#dataPage {
            background: #edf5f1;
        }
        QLabel#dataPageTitle {
            color: #174b35;
            font-size: 24px;
            font-weight: 700;
        }
        QLabel#dataPageSubtitle {
            color: #6a8476;
            font-size: 12px;
        }
        QFrame#dataToolbar {
            background: #ffffff;
            border: 1px solid #d5e7dc;
            border-radius: 16px;
        }
        QFrame#recordFilterBar {
            background: #ffffff;
            border: 1px solid #d5e7dc;
            border-radius: 12px;
        }
        QLabel#toolbarTitle {
            color: #2d5e47;
            font-size: 13px;
            font-weight: 600;
        }
        QPushButton {
            min-height: 22px;
            border-radius: 9px;
            padding: 8px 16px;
            background: #0c8558;
            color: #ffffff;
            border: 1px solid #0c8558;
            font-weight: 600;
        }
        QPushButton:hover {
            background: #109b67;
        }
        QPushButton:pressed {
            background: #086b47;
        }
        QDateTimeEdit {
            min-height: 24px;
            padding: 5px 8px;
            color: #234c39;
            background: #ffffff;
            border: 1px solid #c9ded2;
            border-radius: 6px;
        }
        QDateTimeEdit:disabled {
            color: #8aa095;
            background: #eef4f1;
        }
        QCheckBox {
            color: #2d5e47;
            spacing: 6px;
        }
        QListView {
            background: #ffffff;
            border: 1px solid #d5e7dc;
            border-radius: 14px;
            padding: 8px;
            color: #234c39;
            outline: none;
        }
        QListView::item {
            padding: 12px 10px;
            border-radius: 8px;
        }
        QListView::item:hover {
            background: #eff9f3;
        }
        QListView::item:selected {
            background: #dff3e8;
            color: #126b46;
        }
        QSplitter::handle {
            background: #b7d8c7;
            width: 8px;
            margin: 10px 2px;
            border-radius: 4px;
        }
        QLabel#dataStatus {
            color: #557365;
            background: #e5f3eb;
            border: 1px solid #d2e9dc;
            border-radius: 10px;
            padding: 8px 12px;
        }
    )"));

    /* 依赖方向保持 UI -> UserService -> ClientProtocol -> TcpClient，界面不直接操作 Socket。 */
    m_tcpClient = new TcpClient(this);
    m_userService = new UserService(m_tcpClient, this);
    m_loginWidget = new LoginWidget(m_userService, this);
    m_dashboard = new MonitoringDashboard(this);
    /*
     * 摄像头地址属于本机 192.168.2.0/24 网段，默认必须由 Qt 客户端直连；
     * 云台服务端转发只作为部署到同一网段服务器时的显式可选模式。
     * 不再无条件注入转发回调，避免服务器无法访问摄像头时导致“按钮能点但不动”。
     */
    if (qEnvironmentVariable("SMARTHOME_PTZ_TRANSPORT").compare(
            QStringLiteral("server"), Qt::CaseInsensitive) == 0) {
        m_dashboard->setControlForwarder(
            [this](const QString &cameraUrl, const QString &direction, const QString &move) {
                m_userService->sendPtzControl(cameraUrl, direction, move);
            });
    }
    m_dataPage = createDataPage();
    /*
     * 数据页创建时已经以 MainWindow 为父对象，但此时它还不是中央控件。
     * 如果不主动隐藏，Qt 会按 QWidget 的默认 100x30 几何把它显示在客户区左上角，
     * 从而形成一块与登录页纯白背景颜色不同的浅色方框。
     */
    m_dataPage->hide();
    m_dashboard->hide();
    setCentralWidget(m_loginWidget);

    connect(m_userService, &UserService::loginSuccess, this, &MainWindow::showDataPage);
    connect(m_userService, &UserService::deviceListReceived, this, &MainWindow::updateDevices);
    connect(m_userService, &UserService::recordListReceived, this, &MainWindow::updateRecords);
    connect(m_userService, &UserService::requestFailed, this, &MainWindow::showRequestError);
    connect(m_dashboard, &MonitoringDashboard::requestDeviceList,
            this, &MainWindow::requestDevices);
    connect(m_dashboard, &MonitoringDashboard::requestRecordList,
            this, &MainWindow::requestRecordsForDevice);
    connect(m_dashboard, &MonitoringDashboard::requestPlayback,
            this, &MainWindow::handlePlaybackRequest);
    connect(m_dashboard, &MonitoringDashboard::requestRecordStart,
            this, &MainWindow::handleRecordStart);
    connect(m_dashboard, &MonitoringDashboard::requestRecordStop,
            this, &MainWindow::handleRecordStop);
    connect(m_userService, &UserService::recordingStarted, this, [this]() {
        m_dashboard->setRecordingActive(true);
        m_dataStatus->setText(QStringLiteral("录像已开始"));
    });
    connect(m_userService, &UserService::recordingStopped, this, [this]() {
        m_dashboard->setRecordingActive(false);
        m_dataStatus->setText(QStringLiteral("录像已停止"));
    });

    /* 录像回放：ffmpeg.exe 把本地 TS/MP4 解码成 JPEG -> 显示到通道 01。 */
    m_playbackPlayer = new FilePlaybackPlayer(this);
    connect(m_playbackPlayer, &FilePlaybackPlayer::frameReady, this,
            [this](const QImage &frame) {
        const QList<VideoWidget *> widgets = m_dashboard->videoWidgets();
        if (!widgets.isEmpty()) {
            widgets.at(0)->setFrame(frame);
        }
    });
    connect(m_playbackPlayer, &FilePlaybackPlayer::stateChanged, this,
            [this](const QString &state) {
        const QList<VideoWidget *> widgets = m_dashboard->videoWidgets();
        if (!widgets.isEmpty()) {
            widgets.at(0)->setState(state);
        }
        m_dataStatus->setText(state);
    });
    connect(m_playbackPlayer, &FilePlaybackPlayer::errorOccurred, this,
            [this](const QString &reason) {
        const QList<VideoWidget *> widgets = m_dashboard->videoWidgets();
        if (!widgets.isEmpty()) {
            widgets.at(0)->setState(QStringLiteral("异常"), reason);
        }
        m_dataStatus->setText(reason);
    });

    /*
     * 服务器转发后端（与 RtspPlayer 直连并存，默认不启动）：
     * UserService 从混合 TCP 流里切出媒体帧 -> ServerStreamPlayer 重组 + 解码 -> 显示。
     */
    m_serverStreamPlayer = new ServerStreamPlayer(makeStreamDecoder(), this);
    connect(m_serverStreamPlayer, &ServerStreamPlayer::frameReady, this,
            [this](const QImage &frame) {
        const QList<VideoWidget *> widgets = m_dashboard->videoWidgets();
        if (!widgets.isEmpty()) {
            widgets.at(0)->setFrame(frame);
        }
    });
    connect(m_serverStreamPlayer, &ServerStreamPlayer::stateChanged, this,
            [this](const QString &state) {
        const QList<VideoWidget *> widgets = m_dashboard->videoWidgets();
        if (!widgets.isEmpty()) {
            widgets.at(0)->setState(state);
        }
    });
    connect(m_serverStreamPlayer, &ServerStreamPlayer::errorOccurred, this,
            [this](const QString &reason) {
        const QList<VideoWidget *> widgets = m_dashboard->videoWidgets();
        if (!widgets.isEmpty()) {
            widgets.at(0)->setState(QStringLiteral("异常"), reason);
        }
        m_dataStatus->setText(reason);
    });
    connect(m_userService, &UserService::mediaFrameReceived,
            m_serverStreamPlayer, &ServerStreamPlayer::onMediaFrame);
    connect(m_tcpClient, &TcpClient::disconnected, this, [this]() {
        if (m_serverStreamPlayer != nullptr) {
            m_serverStreamPlayer->stop();
        }
    });

    /*
     * 默认使用本机端点，避免把某台开发机地址固化到可执行文件；部署到 ECS 或
     * 局域网时通过 SMARTHOME_SERVER_IP/SMARTHOME_SERVER_PORT 覆盖，实际网络读写
     * 仍由 TcpClient 负责。
     */
    const ServerEndpoint endpoint = resolveServerEndpoint();
    m_tcpClient->connectServer(endpoint.host, endpoint.port);
}

QWidget *MainWindow::createDataPage()
{
    QWidget *page = new QWidget(this);
    page->setObjectName(QStringLiteral("dataPage"));
    QVBoxLayout *rootLayout = new QVBoxLayout(page);
    rootLayout->setContentsMargins(28, 24, 28, 24);
    rootLayout->setSpacing(14);
    QLabel *title = new QLabel(QStringLiteral("设备与录像元数据"), page);
    title->setObjectName(QStringLiteral("dataPageTitle"));
    rootLayout->addWidget(title);

    QLabel *subtitle = new QLabel(QStringLiteral("查看家庭设备状态，并按设备查询录像记录"), page);
    subtitle->setObjectName(QStringLiteral("dataPageSubtitle"));
    rootLayout->addWidget(subtitle);

    QFrame *toolbar = new QFrame(page);
    toolbar->setObjectName(QStringLiteral("dataToolbar"));
    QHBoxLayout *buttonLayout = new QHBoxLayout(toolbar);
    buttonLayout->setContentsMargins(16, 10, 16, 10);
    QLabel *toolbarTitle = new QLabel(QStringLiteral("数据操作"), toolbar);
    toolbarTitle->setObjectName(QStringLiteral("toolbarTitle"));
    QPushButton *deviceButton = new QPushButton(QStringLiteral("获取设备列表"), toolbar);
    m_recordQueryButton = new QPushButton(QStringLiteral("查询所选设备录像"), toolbar);
    buttonLayout->addWidget(toolbarTitle);
    buttonLayout->addSpacing(14);
    buttonLayout->addWidget(deviceButton);
    buttonLayout->addWidget(m_recordQueryButton);
    buttonLayout->addStretch();
    rootLayout->addWidget(toolbar);

    /*
     * 录像时间条件属于 B 的元数据查询接口，不涉及录像文件创建或播放。
     * 默认查询全部时间，用户取消勾选后可输入起止时间；两端统一使用
     * yyyy-MM-dd HH:mm:ss，避免本地化显示格式导致服务端 DATETIME 解析不一致。
     */
    QFrame *filterBar = new QFrame(page);
    filterBar->setObjectName(QStringLiteral("recordFilterBar"));
    QHBoxLayout *filterLayout = new QHBoxLayout(filterBar);
    filterLayout->setContentsMargins(14, 8, 14, 8);
    filterLayout->setSpacing(8);
    QLabel *filterTitle = new QLabel(QStringLiteral("录像时间"), filterBar);
    m_allTimeCheckBox = new QCheckBox(QStringLiteral("全部时间"), filterBar);
    m_allTimeCheckBox->setObjectName(QStringLiteral("allTimeCheckBox"));
    m_allTimeCheckBox->setChecked(true);

    const QDateTime now = QDateTime::currentDateTime();
    m_recordStartEdit = new QDateTimeEdit(now.addDays(-1), filterBar);
    m_recordStartEdit->setObjectName(QStringLiteral("recordStartEdit"));
    m_recordStartEdit->setDisplayFormat(QStringLiteral("yyyy-MM-dd HH:mm:ss"));
    m_recordStartEdit->setCalendarPopup(true);
    m_recordStartEdit->setEnabled(false);
    m_recordEndEdit = new QDateTimeEdit(now, filterBar);
    m_recordEndEdit->setObjectName(QStringLiteral("recordEndEdit"));
    m_recordEndEdit->setDisplayFormat(QStringLiteral("yyyy-MM-dd HH:mm:ss"));
    m_recordEndEdit->setCalendarPopup(true);
    m_recordEndEdit->setEnabled(false);

    filterLayout->addWidget(filterTitle);
    filterLayout->addWidget(m_allTimeCheckBox);
    filterLayout->addSpacing(8);
    filterLayout->addWidget(new QLabel(QStringLiteral("开始"), filterBar));
    filterLayout->addWidget(m_recordStartEdit);
    filterLayout->addWidget(new QLabel(QStringLiteral("结束"), filterBar));
    filterLayout->addWidget(m_recordEndEdit);
    filterLayout->addStretch();
    rootLayout->addWidget(filterBar);

    connect(m_allTimeCheckBox, &QCheckBox::toggled, this, [this](bool allTime) {
        /* “全部时间”不发送人为边界，服务端会使用安全的 DATETIME 默认范围。 */
        m_recordStartEdit->setEnabled(!allTime);
        m_recordEndEdit->setEnabled(!allTime);
    });

    /* 两个列表并排呈现，设备选择决定录像查询使用的 deviceId。 */
    QSplitter *splitter = new QSplitter(page);
    m_deviceList = new QListView(splitter);
    m_deviceList->setObjectName(QStringLiteral("deviceList"));
    QListView *recordList = new QListView(splitter);
    recordList->setObjectName(QStringLiteral("recordList"));
    m_deviceModel = new DeviceModel(this);
    m_recordModel = new RecordModel(this);
    m_deviceList->setModel(m_deviceModel);
    recordList->setModel(m_recordModel);
    splitter->setHandleWidth(8);
    splitter->setStretchFactor(0, 1);
    splitter->setStretchFactor(1, 1);
    splitter->addWidget(m_deviceList);
    splitter->addWidget(recordList);
    rootLayout->addWidget(splitter, 1);

    m_dataStatus = new QLabel(QStringLiteral("登录后可获取设备和录像元数据。"), page);
    m_dataStatus->setObjectName(QStringLiteral("dataStatus"));
    m_dataStatus->setWordWrap(true);
    rootLayout->addWidget(m_dataStatus);

    connect(deviceButton, &QPushButton::clicked, this, &MainWindow::requestDevices);
    connect(m_recordQueryButton, &QPushButton::clicked, this, &MainWindow::requestRecords);
    return page;
}

void MainWindow::showDataPage(quint64 userId)
{
    /* 登录成功仅切换到元数据页；用户可随后主动发起每个数据请求。 */
    m_dataPage->hide();
    setCentralWidget(m_dashboard);
    m_dashboard->show();

    QString configPath = qEnvironmentVariable("SMARTHOME_CAMERA_CONFIG");
    if (configPath.isEmpty()) {
        configPath = QCoreApplication::applicationDirPath()
            + QStringLiteral("/conf/cameras.local.conf");
    }
    QString configError;
    const QList<CameraConfig> configs = loadCameraConfigs(configPath, &configError);
    m_dashboard->setCameraConfigs(configs);
    const bool useServerStream = qEnvironmentVariableIntValue("SMARTHOME_USE_SERVER_STREAM") != 0;
    if (!useServerStream) {
        /* 服务端转发模式由 ServerStreamPlayer 独占首路，避免直连和转发叠加绘制。 */
        m_dashboard->startPreview();
    }

    /*
     * 可选：SMARTHOME_USE_SERVER_STREAM=1 时走「服务器转发 → FFmpeg 解码」链路，
     * 用第一路启用摄像头的 RTSP 地址（或 SMARTHOME_STREAM_URL）请求服务器推流。
     * 默认不开启，保持原有 RtspPlayer 直连方案。
     */
    if (useServerStream) {
        QString streamUrl = qEnvironmentVariable("SMARTHOME_STREAM_URL");
        if (streamUrl.isEmpty()) {
            for (const CameraConfig &config : configs) {
                if (config.enabled && !config.rtspUrl.isEmpty()) {
                    streamUrl = config.rtspUrl;
                    break;
                }
            }
        }
        if (m_serverStreamPlayer != nullptr) {
            m_serverStreamPlayer->start();
            m_userService->startStream(streamUrl);
        }
    }
    if (!configError.isEmpty()) {
        m_dataStatus->setText(QStringLiteral("用户 %1 已登录；%2").arg(QString::number(userId), configError));
    } else {
        m_dataStatus->setText(QStringLiteral("用户 %1 已登录，监控工作台已启动").arg(userId));
    }
    /* 登录成功后自动请求设备列表，设备树和旧数据模型同时获得服务端数据。 */
    m_userService->requestDeviceList();
}

void MainWindow::requestDevices()
{
    m_dataStatus->setText(QStringLiteral("正在请求设备列表…"));
    m_userService->requestDeviceList();
}

void MainWindow::requestRecords()
{
    const QModelIndex selected = m_deviceList->currentIndex();
    if (!selected.isValid()) {
        m_dataStatus->setText(QStringLiteral("请先在左侧选择一个设备。"));
        return;
    }
    requestRecordsForDevice(m_deviceModel->deviceIdAt(selected.row()));
}

void MainWindow::requestRecordsForDevice(quint64 deviceId)
{
    /*
     * 登录后的监控工作台和旧数据页都复用同一条业务入口，区别只在于
     * deviceId 的来源：工作台来自服务端设备树，数据页来自 QListView。
     * 这样既修复工作台查询，又保留原有 B 数据页按钮行为。
     */
    if (deviceId == 0) {
        m_dataStatus->setText(QStringLiteral("请先在设备列表中选择一个服务端设备。"));
        return;
    }
    QString startTime;
    QString endTime;
    if (!m_allTimeCheckBox->isChecked()) {
        if (m_recordStartEdit->dateTime() > m_recordEndEdit->dateTime()) {
            m_dataStatus->setText(QStringLiteral("开始时间不能晚于结束时间。"));
            return;
        }
        const QString wireFormat = QStringLiteral("yyyy-MM-dd HH:mm:ss");
        startTime = m_recordStartEdit->dateTime().toString(wireFormat);
        endTime = m_recordEndEdit->dateTime().toString(wireFormat);
    }

    /* 空字符串明确表达全部时间；指定范围则使用服务端可直接比较的固定格式。 */
    m_dataStatus->setText(QStringLiteral("正在请求录像元数据…"));
    m_userService->requestRecordQuery(deviceId, startTime, endTime);
}

void MainWindow::updateDevices(const QList<ClientProtocol::DeviceInfo> &devices)
{
    m_deviceModel->setDevices(devices);
    m_dashboard->setDevices(devices);
    m_dataStatus->setText(QStringLiteral("已收到 %1 个设备。").arg(devices.size()));
}

void MainWindow::updateRecords(const QList<ClientProtocol::RecordInfo> &records)
{
    m_records = records;
    m_recordModel->setRecords(records);
    m_dataStatus->setText(QStringLiteral("已收到 %1 条录像元数据。").arg(records.size()));

    /* 回放请求先触发一次查询，拿到结果后播放最近一条录像。 */
    if (m_pendingPlayback) {
        m_pendingPlayback = false;
        if (!m_records.isEmpty() && m_playbackPlayer != nullptr) {
            m_playbackPlayer->play(m_records.first().filePath);
        } else {
            m_dataStatus->setText(QStringLiteral("没有可回放的录像。"));
        }
    }
}

void MainWindow::handlePlaybackRequest(quint64 deviceId)
{
    if (deviceId == 0) {
        m_dataStatus->setText(QStringLiteral("请先在设备列表中选择一个服务端设备。"));
        return;
    }
    m_pendingPlayback = true;
    requestRecordsForDevice(deviceId);
}

void MainWindow::showRequestError(const QString &reason)
{
    m_dataStatus->setText(reason);
}

void MainWindow::handleRecordStart(quint64 deviceId)
{
    if (deviceId == 0) {
        m_dataStatus->setText(QStringLiteral("请先在设备列表中选择一个服务端设备。"));
        return;
    }
    m_dataStatus->setText(QStringLiteral("正在启动录像…"));
    m_userService->startRecording(deviceId);
}

void MainWindow::handleRecordStop()
{
    m_dataStatus->setText(QStringLiteral("正在停止录像…"));
    m_userService->stopRecording();
}

MainWindow::~MainWindow()
{
    /* 业务对象均拥有 MainWindow 父对象；ui 不是 QObject，需显式释放。 */
    /* 先停止工作台中的 FFmpeg 子进程，再释放 Qt 对象，避免残留视频进程。 */
    /*
     * 播放器属于 MainWindow 的子对象，而 Qt 销毁子对象时不保证
     * m_dashboard 先于播放器保留。播放器 stop() 会发出 stateChanged，
     * 若此时回调再次访问已销毁的工作台就会触发退出阶段崩溃；因此先断开
     * 这些只服务于界面刷新的连接，再让 QObject 统一回收子对象。
     */
    if (m_playbackPlayer != nullptr) {
        disconnect(m_playbackPlayer, nullptr, this, nullptr);
        m_playbackPlayer->stop();
    }
    if (m_serverStreamPlayer != nullptr) {
        disconnect(m_serverStreamPlayer, nullptr, this, nullptr);
        m_serverStreamPlayer->stop();
    }
    if (m_dashboard != nullptr) {
        m_dashboard->stopPreview();
    }
    delete ui;
}
