#include "MonitoringDashboard.h"

#include <QEvent>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QListWidgetItem>
#include <QPushButton>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QVBoxLayout>

#include "video/RtspPlayer.h"
#include "video/VideoWidget.h"
#ifdef SMART_HOME_WITH_VLC
#include "video/VlcPlayer.h"
#endif

namespace {

QPushButton *makeNavButton(const QString &text, QWidget *parent)
{
    QPushButton *button = new QPushButton(text, parent);
    button->setMinimumSize(92, 34);
    button->setMaximumWidth(118);
    button->setCursor(Qt::PointingHandCursor);
    return button;
}

}

MonitoringDashboard::MonitoringDashboard(QWidget *parent)
    : QWidget(parent),
      m_eventList(nullptr),
      m_statusLabel(nullptr),
      m_deviceTree(nullptr),
      m_ptzPanel(nullptr),
      m_recordControlButton(nullptr),
      m_recordingActive(false),
      m_ptzClient(new PtzClient(this)),
      m_gunItem(nullptr),
      m_domeItem(nullptr)
{
    buildUi();
    connect(m_deviceTree, &QTreeWidget::itemSelectionChanged,
            this, &MonitoringDashboard::handleDeviceSelection);
    connect(m_ptzClient, &PtzClient::ptzReady,
            this, &MonitoringDashboard::handlePtzReady);
    connect(m_ptzClient, &PtzClient::errorOccurred,
            this, &MonitoringDashboard::handlePtzError);
}

MonitoringDashboard::~MonitoringDashboard()
{
    stopPreview();
}

void MonitoringDashboard::buildUi()
{
    setObjectName(QStringLiteral("monitoringDashboard"));
    setMinimumSize(1050, 650);
    setStyleSheet(QStringLiteral(R"(
        QWidget#monitoringDashboard { background: #202832; color: #e9edf2; font-family: "Segoe UI"; }
        QFrame#dashboardHeader { background: #2b3440; border-bottom: 1px solid #465261; }
        QPushButton { background: #374352; border: 1px solid #536174; border-radius: 5px; color: #e9edf2; padding: 6px 12px; font-size: 13px; }
        QPushButton:hover { background: #465363; }
        QPushButton:pressed { background: #237da7; }
        QPushButton:disabled { background: #303944; color: #68717e; border-color: #3b4653; }
        QPushButton#activeNavButton { background: #3b91bd; border-color: #6ec2e8; }
        QListWidget, QTreeWidget { background: #2d3642; border: 1px solid #465261; border-radius: 5px; color: #e1e6ec; }
        QListWidget::item, QTreeWidget::item { padding: 3px 6px; }
        QListWidget::item:hover, QTreeWidget::item:hover { background: #394758; }
        QListWidget::item:selected, QTreeWidget::item:selected { background: #3b91bd; color: white; }
        QGroupBox { border: 1px solid #465261; border-radius: 6px; margin-top: 10px; padding-top: 8px; color: #e1e6ec; font-weight: 600; }
        QGroupBox::title { subcontrol-origin: margin; left: 10px; padding: 0 5px; }
        QLabel#panelCaption { color: #aeb8c4; font-size: 11px; }
        QLabel#ptzHintLabel { color: #9da9b7; font-size: 11px; }
        QLabel#dashboardStatus { background: #2b3440; border-top: 1px solid #465261; color: #aeb8c4; padding: 5px 10px; min-height: 20px; }
    )"));

    QVBoxLayout *root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    QFrame *header = new QFrame(this);
    header->setObjectName(QStringLiteral("dashboardHeader"));
    header->setFixedHeight(56);
    QHBoxLayout *headerLayout = new QHBoxLayout(header);
    headerLayout->setContentsMargins(12, 8, 12, 8);
    headerLayout->setSpacing(8);

    /* 顶部只保留当前页面的业务导航，不再重复显示登录页中的品牌标题，
       这样可以把有限的窗口高度留给实时预览和设备操作区域。 */
    headerLayout->addStretch();
    QPushButton *previewButton = makeNavButton(QStringLiteral("实时预览"), header);
    previewButton->setObjectName(QStringLiteral("activeNavButton"));
    QPushButton *recordButton = makeNavButton(QStringLiteral("录像查询"), header);
    QPushButton *playbackButton = makeNavButton(QStringLiteral("回放"), header);
    QPushButton *deviceButton = makeNavButton(QStringLiteral("设备数据"), header);
    m_recordControlButton = makeNavButton(QStringLiteral("开始录像"), header);
    m_recordControlButton->setObjectName(QStringLiteral("recordControlButton"));
    headerLayout->addWidget(previewButton);
    headerLayout->addWidget(recordButton);
    headerLayout->addWidget(playbackButton);
    headerLayout->addWidget(deviceButton);
    headerLayout->addWidget(m_recordControlButton);
    root->addWidget(header);

    QHBoxLayout *content = new QHBoxLayout;
    content->setContentsMargins(10, 8, 10, 6);
    content->setSpacing(10);

    QGroupBox *events = new QGroupBox(QStringLiteral("状态信息"), this);
    events->setObjectName(QStringLiteral("eventPanel"));
    events->setMinimumWidth(190);
    events->setMaximumWidth(230);
    QVBoxLayout *eventLayout = new QVBoxLayout(events);
    eventLayout->setContentsMargins(8, 8, 8, 8);
    eventLayout->setSpacing(5);
    QLabel *eventCaption = new QLabel(QStringLiteral("最近事件"), events);
    eventCaption->setObjectName(QStringLiteral("panelCaption"));
    m_eventList = new QListWidget(events);
    m_eventList->setObjectName(QStringLiteral("eventList"));
    m_eventList->setSelectionMode(QAbstractItemView::NoSelection);
    m_eventList->setUniformItemSizes(true);
    m_eventList->setWordWrap(false);
    m_eventList->setSpacing(1);
    m_eventList->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_eventList->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    eventLayout->addWidget(eventCaption);
    eventLayout->addWidget(m_eventList, 1);
    content->addWidget(events);

    QWidget *videoArea = new QWidget(this);
    videoArea->setObjectName(QStringLiteral("videoArea"));
    QGridLayout *videoGrid = new QGridLayout(videoArea);
    videoGrid->setContentsMargins(0, 0, 0, 0);
    videoGrid->setSpacing(8);
    const QStringList slotNames = QStringList()
        << QStringLiteral("通道 01 · 枪机")
        << QStringLiteral("通道 02 · 球机")
        << QStringLiteral("通道 03 · 等待接入")
        << QStringLiteral("通道 04 · 等待接入");
    for (int index = 0; index < 4; ++index) {
        VideoWidget *video = new VideoWidget(videoArea);
        video->setObjectName(QStringLiteral("channelVideo%1").arg(index + 1));
        video->setChannelName(slotNames.at(index));
        if (index >= 2) {
            video->setState(QStringLiteral("等待接入"));
        }
        m_videoWidgets.append(video);
        videoGrid->addWidget(video, index / 2, index % 2);
    }
    videoArea->setMinimumWidth(560);
    videoGrid->setRowStretch(0, 1);
    videoGrid->setRowStretch(1, 1);
    videoGrid->setColumnStretch(0, 1);
    videoGrid->setColumnStretch(1, 1);
    content->addWidget(videoArea, 1);

    QWidget *rightPanel = new QWidget(this);
    rightPanel->setMinimumWidth(248);
    rightPanel->setMaximumWidth(300);
    QVBoxLayout *rightLayout = new QVBoxLayout(rightPanel);
    rightLayout->setContentsMargins(0, 0, 0, 0);
    rightLayout->setSpacing(8);
    QGroupBox *devicePanel = new QGroupBox(QStringLiteral("设备列表"), rightPanel);
    devicePanel->setObjectName(QStringLiteral("devicePanel"));
    devicePanel->setMinimumHeight(280);
    QVBoxLayout *deviceLayout = new QVBoxLayout(devicePanel);
    m_deviceTree = new QTreeWidget(devicePanel);
    m_deviceTree->setObjectName(QStringLiteral("monitorDeviceTree"));
    m_deviceTree->setHeaderHidden(true);
    m_gunItem = new QTreeWidgetItem(m_deviceTree, QStringList() << QStringLiteral("枪机 · 通道 01"));
    m_gunItem->setData(0, Qt::UserRole, QStringLiteral("gun"));
    m_domeItem = new QTreeWidgetItem(m_deviceTree, QStringList() << QStringLiteral("球机 · 通道 02"));
    m_domeItem->setData(0, Qt::UserRole, QStringLiteral("dome"));
    deviceLayout->addWidget(m_deviceTree);
    rightLayout->addWidget(devicePanel, 1);

    m_ptzPanel = new QGroupBox(QStringLiteral("云台控制 · 球机"), rightPanel);
    m_ptzPanel->setObjectName(QStringLiteral("ptzPanel"));
    m_ptzPanel->setMinimumHeight(200);
    m_ptzPanel->setMaximumHeight(230);
    rightLayout->addWidget(m_ptzPanel, 0);
    buildPtzControls();
    content->addWidget(rightPanel);
    root->addLayout(content, 1);

    m_statusLabel = new QLabel(QStringLiteral("就绪 · 登录成功，等待摄像头配置"), this);
    m_statusLabel->setObjectName(QStringLiteral("dashboardStatus"));
    root->addWidget(m_statusLabel);

    connect(previewButton, &QPushButton::clicked, this, [this]() {
        startPreview();
        appendEvent(QStringLiteral("已切换到实时预览"));
    });
    connect(recordButton, &QPushButton::clicked, this, [this]() {
        /*
         * 设备树和 B 数据页使用的是两套展示控件；这里直接读取工作台当前
         * 服务端子项的 UserRole+1，避免 MainWindow 再去访问隐藏的数据页列表。
         * 未选中服务端设备时发送 0，由 MainWindow 统一给出选择提示。
         */
        emit requestRecordList(selectedServerDeviceId());
        appendEvent(QStringLiteral("录像查询入口已就绪"));
    });
    connect(playbackButton, &QPushButton::clicked, this, [this]() {
        emit requestPlayback(selectedServerDeviceId());
        appendEvent(QStringLiteral("正在请求录像回放"));
    });
    connect(deviceButton, &QPushButton::clicked, this, [this]() {
        emit requestDeviceList();
        appendEvent(QStringLiteral("正在请求设备列表"));
    });
    connect(m_recordControlButton, &QPushButton::clicked, this, [this]() {
        if (m_recordingActive) {
            emit requestRecordStop();
            appendEvent(QStringLiteral("正在停止录像"));
            return;
        }
        emit requestRecordStart(selectedServerDeviceId());
        appendEvent(QStringLiteral("正在请求开始录像"));
    });

    m_deviceTree->setCurrentItem(m_gunItem);
    setPtzButtonsEnabled(false);
}

void MonitoringDashboard::buildPtzControls()
{
    QGridLayout *layout = new QGridLayout(m_ptzPanel);
    layout->setContentsMargins(10, 18, 10, 10);
    layout->setSpacing(4);

    const QList<PtzClient::Direction> directions = QList<PtzClient::Direction>()
        << PtzClient::Direction::UpLeft << PtzClient::Direction::Up << PtzClient::Direction::UpRight
        << PtzClient::Direction::Left << PtzClient::Direction::Right
        << PtzClient::Direction::DownLeft << PtzClient::Direction::Down << PtzClient::Direction::DownRight;
    const QStringList labels = QStringList() << QStringLiteral("↖") << QStringLiteral("↑") << QStringLiteral("↗")
        << QStringLiteral("←") << QStringLiteral("→") << QStringLiteral("↙")
        << QStringLiteral("↓") << QStringLiteral("↘");
    const QList<QPair<int, int> > cells = QList<QPair<int, int> >()
        << qMakePair(0, 0) << qMakePair(0, 1) << qMakePair(0, 2)
        << qMakePair(1, 0) << qMakePair(1, 2)
        << qMakePair(2, 0) << qMakePair(2, 1) << qMakePair(2, 2);

    for (int index = 0; index < directions.size(); ++index) {
        QPushButton *button = new QPushButton(labels.at(index), m_ptzPanel);
        button->setObjectName(QStringLiteral("ptzDirectionButton%1").arg(index));
        button->setMinimumSize(38, 30);
        button->setProperty("ptzDirection", static_cast<int>(directions.at(index)));
        button->installEventFilter(this);
        m_ptzButtons.append(button);
        layout->addWidget(button, cells.at(index).first, cells.at(index).second);
    }

    QPushButton *stopButton = new QPushButton(QStringLiteral("■"), m_ptzPanel);
    stopButton->setObjectName(QStringLiteral("ptzStopButton"));
    stopButton->setMinimumSize(38, 30);
    connect(stopButton, &QPushButton::clicked, m_ptzClient, &PtzClient::stopMove);
    m_auxPtzButtons.append(stopButton);
    layout->addWidget(stopButton, 1, 1);

    /* 需求一期只要求八方向和停止；变倍/聚焦等镜头动作不在本次已验证接口范围内，
       因此不创建“看似可用但没有协议实现”的按钮，避免误导现场操作人员。 */
    QLabel *hint = new QLabel(QStringLiteral("按住方向键移动，松开即停止"), m_ptzPanel);
    hint->setObjectName(QStringLiteral("ptzHintLabel"));
    hint->setWordWrap(true);
    layout->addWidget(hint, 3, 0, 1, 3);
}

void MonitoringDashboard::setCameraConfigs(const QList<CameraConfig> &configs)
{
    stopPreview();
    qDeleteAll(m_players);
    m_players.clear();
#ifdef SMART_HOME_WITH_VLC
    qDeleteAll(m_vlcPlayers);
    m_vlcPlayers.clear();
#endif
    m_cameraConfigs = configs;
    QList<bool> usedSlots;
    usedSlots << false << false << false << false;

    for (const CameraConfig &config : configs) {
        if (!config.enabled || config.rtspUrl.isEmpty()) {
            continue;
        }
        const int slot = slotForConfig(config, usedSlots);
        if (slot < 0) {
            appendEvent(QStringLiteral("忽略超出四宫格的摄像头配置：%1").arg(config.name));
            continue;
        }
#ifdef SMART_HOME_WITH_VLC
        /*
         * VLC 后端：libvlc 直接渲染到 VideoWidget 的原生窗口，无需帧转码。
         * 仅在 WITH_VLC=ON 时参与编译；默认仍走下方 ffmpeg 的 RtspPlayer。
         */
        VlcPlayer *player = new VlcPlayer(this);
        VideoWidget *video = m_videoWidgets.at(slot);
        video->setNativeVideoMode(true);
        connect(player, &VlcPlayer::stateChanged, this, [this, slot](const QString &state) {
            if (slot >= 0 && slot < m_videoWidgets.size()) {
                m_videoWidgets.at(slot)->setState(state);
            }
        });
        connect(player, &VlcPlayer::errorOccurred, this, [this, slot](const QString &reason) {
            if (slot >= 0 && slot < m_videoWidgets.size()) {
                m_videoWidgets.at(slot)->setState(QStringLiteral("异常"), reason);
            }
            appendEvent(reason);
        });
        if (player->init()) {
            player->playUrl(config.rtspUrl, reinterpret_cast<void *>(video->winId()));
        }
        m_vlcPlayers.append(player);
#else
        RtspPlayer *player = new RtspPlayer(this);
        player->setSource(config.rtspUrl, config.ffmpegPath);
        connect(player, &RtspPlayer::frameReady, this, [this, slot](const QImage &frame) {
            if (slot >= 0 && slot < m_videoWidgets.size()) {
                m_videoWidgets.at(slot)->setFrame(frame);
            }
        });
        connect(player, &RtspPlayer::stateChanged, this, [this, slot](const QString &state) {
            if (slot >= 0 && slot < m_videoWidgets.size()) {
                m_videoWidgets.at(slot)->setState(state);
            }
        });
        connect(player, &RtspPlayer::errorOccurred, this, [this, slot](const QString &reason) {
            if (slot >= 0 && slot < m_videoWidgets.size()) {
                m_videoWidgets.at(slot)->setState(QStringLiteral("异常"), reason);
            }
            appendEvent(reason);
        });
        m_players.append(player);
#endif
        appendEvent(QStringLiteral("已加载摄像头配置：%1").arg(config.name));
    }
}

void MonitoringDashboard::setControlForwarder(
    const std::function<void(const QString &, const QString &, const QString &)> &forwarder)
{
    /*
     *这里只保存备用转发入口，不能立即覆盖 PtzClient 的直连行为。
     * 具体选择 direct/server 必须等用户选中球机并读取该设备配置后决定。
     */
    m_controlForwarder = forwarder;
}

void MonitoringDashboard::setDevices(const QList<ClientProtocol::DeviceInfo> &devices)
{
    m_devices = devices;
    QTreeWidgetItem *serverRoot = m_deviceTree->findItems(QStringLiteral("服务端设备"), Qt::MatchExactly).value(0);
    if (serverRoot == nullptr) {
        serverRoot = new QTreeWidgetItem(m_deviceTree, QStringList() << QStringLiteral("服务端设备"));
    }
    serverRoot->takeChildren();
    for (const ClientProtocol::DeviceInfo &device : devices) {
        QTreeWidgetItem *item = new QTreeWidgetItem(serverRoot,
            QStringList() << QStringLiteral("%1 · %2").arg(device.name, device.status));
        item->setData(0, Qt::UserRole, device.type);
        item->setData(0, Qt::UserRole + 1, QVariant::fromValue<qulonglong>(device.id));
    }
    serverRoot->setExpanded(true);
    m_statusLabel->setText(QStringLiteral("已同步 %1 个服务端设备").arg(devices.size()));
}

void MonitoringDashboard::setRecordingActive(bool active)
{
    m_recordingActive = active;
    if (m_recordControlButton != nullptr) {
        m_recordControlButton->setText(active ? QStringLiteral("停止录像")
                                               : QStringLiteral("开始录像"));
    }
    if (m_statusLabel != nullptr) {
        m_statusLabel->setText(active ? QStringLiteral("录像进行中")
                                      : QStringLiteral("录像已停止"));
    }
}

void MonitoringDashboard::startPreview()
{
#ifdef SMART_HOME_WITH_VLC
    /* VLC 后端在 setCameraConfigs 中已启动，这里只需从暂停/停止状态恢复。 */
    if (!m_vlcPlayers.isEmpty()) {
        for (VlcPlayer *player : m_vlcPlayers) {
            if (player != nullptr) {
                player->play();
            }
        }
        m_statusLabel->setText(QStringLiteral("正在连接本地 RTSP 摄像头"));
        return;
    }
#endif
    if (m_players.isEmpty()) {
        m_statusLabel->setText(QStringLiteral("未找到启用的本地摄像头配置"));
        appendEvent(QStringLiteral("请先配置 conf/cameras.local.conf"));
        return;
    }
    for (RtspPlayer *player : m_players) {
        player->start();
    }
    m_statusLabel->setText(QStringLiteral("正在连接本地 RTSP 摄像头"));
}

void MonitoringDashboard::stopPreview()
{
    for (RtspPlayer *player : m_players) {
        if (player != nullptr) {
            player->stop();
        }
    }
#ifdef SMART_HOME_WITH_VLC
    for (VlcPlayer *player : m_vlcPlayers) {
        if (player != nullptr) {
            player->stop();
        }
    }
#endif
}

QList<VideoWidget *> MonitoringDashboard::videoWidgets() const
{
    return m_videoWidgets;
}

QTreeWidget *MonitoringDashboard::deviceTree() const
{
    return m_deviceTree;
}

QGroupBox *MonitoringDashboard::ptzPanel() const
{
    return m_ptzPanel;
}

bool MonitoringDashboard::eventFilter(QObject *watched, QEvent *event)
{
    QPushButton *button = qobject_cast<QPushButton *>(watched);
    if (button != nullptr && m_ptzButtons.contains(button)) {
        if (event->type() == QEvent::MouseButtonPress) {
            const PtzClient::Direction direction = static_cast<PtzClient::Direction>(
                button->property("ptzDirection").toInt());
            m_ptzClient->startMove(direction);
        } else if (event->type() == QEvent::MouseButtonRelease
                   || event->type() == QEvent::Leave
                   || event->type() == QEvent::FocusOut) {
            m_ptzClient->stopMove();
        }
    }
    return QWidget::eventFilter(watched, event);
}

void MonitoringDashboard::handleDeviceSelection()
{
    QTreeWidgetItem *item = m_deviceTree->currentItem();
    applySelectedCamera(item);
}

void MonitoringDashboard::handlePtzReady(bool supported)
{
    setPtzButtonsEnabled(supported);
    m_statusLabel->setText(supported ? QStringLiteral("球机云台已就绪") : QStringLiteral("当前设备未启用云台"));
}

void MonitoringDashboard::handlePtzError(const QString &reason)
{
    setPtzButtonsEnabled(false);
    appendEvent(reason);
    m_statusLabel->setText(reason);
}

void MonitoringDashboard::appendEvent(const QString &message)
{
    const QString normalized = message.simplified();
    if (m_eventList == nullptr || normalized.isEmpty()) {
        return;
    }

    /* RTSP 重连或播放器错误可能在短时间内连续上报同一文本。
       只合并相邻重复项，既保留事件顺序，又避免左侧面板被刷屏。 */
    if (m_eventList->count() > 0
        && m_eventList->item(m_eventList->count() - 1)->text() == normalized) {
        QListWidgetItem *lastItem = m_eventList->item(m_eventList->count() - 1);
        lastItem->setData(Qt::UserRole, lastItem->data(Qt::UserRole).toInt() + 1);
        return;
    }

    QListWidgetItem *item = new QListWidgetItem(normalized, m_eventList);
    item->setData(Qt::UserRole, 1);
    while (m_eventList->count() > 24) {
        delete m_eventList->takeItem(0);
    }
    m_eventList->scrollToBottom();
    emit eventLogged(normalized);
}

void MonitoringDashboard::applySelectedCamera(QTreeWidgetItem *item)
{
    const QString type = item == nullptr ? QString() : item->data(0, Qt::UserRole).toString();
    setPtzButtonsEnabled(false);
    if (type != QStringLiteral("dome")) {
        /* 退出球机时同时清除目标与转发回调，防止旧设备控制状态残留。 */
        m_ptzClient->setControlForwarder(
            std::function<void(const QString &, const QString &, const QString &)>());
        m_ptzClient->setCamera(QUrl(), QString(), QString());
        m_statusLabel->setText(QStringLiteral("枪机不支持云台控制"));
        return;
    }

    for (const CameraConfig &config : m_cameraConfigs) {
        if (config.type == QStringLiteral("dome")) {
            /*
             * 局域网摄像头默认由本机直连；只有配置显式指定 Server 时，
             * 才使用 MainWindow 保存的 UserService/TLV 转发回调。
             */
            if (config.ptzTransport == CameraConfig::PtzTransport::Server) {
                m_ptzClient->setControlForwarder(m_controlForwarder);
            } else {
                m_ptzClient->setControlForwarder(
                    std::function<void(const QString &, const QString &, const QString &)>());
            }
            m_ptzClient->setCamera(QUrl(config.webUrl), config.user, config.password);
            m_ptzClient->probe();
            m_statusLabel->setText(QStringLiteral("正在探测球机云台能力"));
            return;
        }
    }
    m_ptzClient->setControlForwarder(
        std::function<void(const QString &, const QString &, const QString &)>());
    m_ptzClient->setCamera(QUrl(), QString(), QString());
    m_statusLabel->setText(QStringLiteral("未配置球机 Web 地址"));
}

void MonitoringDashboard::setPtzButtonsEnabled(bool enabled)
{
    for (QPushButton *button : m_ptzButtons) {
        button->setEnabled(enabled);
    }
    for (QPushButton *button : m_auxPtzButtons) {
        button->setEnabled(enabled);
    }
}

quint64 MonitoringDashboard::selectedServerDeviceId() const
{
    /*
     * 枪机/球机两个固定入口没有设备 ID；只有服务端设备树下的子项
     * 才在 UserRole+1 写入协议返回的 deviceId。
     */
    if (m_deviceTree == nullptr) {
        return 0;
    }
    QTreeWidgetItem *item = m_deviceTree->currentItem();
    if (item == nullptr || item->parent() == nullptr) {
        return 0;
    }
    const QVariant idValue = item->data(0, Qt::UserRole + 1);
    return idValue.isValid() ? idValue.toULongLong() : 0;
}

int MonitoringDashboard::slotForConfig(const CameraConfig &config, QList<bool> &usedSlots) const
{
    const int preferred = config.type == QStringLiteral("dome") ? 1
        : (config.type == QStringLiteral("gun") ? 0 : -1);
    if (preferred >= 0 && preferred < usedSlots.size() && !usedSlots.at(preferred)) {
        usedSlots[preferred] = true;
        return preferred;
    }
    for (int index = 0; index < usedSlots.size(); ++index) {
        if (!usedSlots.at(index)) {
            usedSlots[index] = true;
            return index;
        }
    }
    return -1;
}
