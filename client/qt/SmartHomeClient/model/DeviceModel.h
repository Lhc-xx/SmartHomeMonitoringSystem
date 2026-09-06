#ifndef DEVICEMODEL_H
#define DEVICEMODEL_H

#include <QAbstractListModel>

#include "protocol/ClientProtocol.h"

/*
 * DeviceModel 将已解析的设备元数据适配为 QListView 可消费的列表模型。
 * 它只持有设备名称、类型和状态，不涉及实时控制或视频数据。
 */
class DeviceModel : public QAbstractListModel
{
    Q_OBJECT

public:
    /* 自定义角色让界面可分别取得设备标识、名称、类型和在线状态。 */
    enum DeviceRole { DeviceIdRole = Qt::UserRole + 1, DeviceNameRole, DeviceTypeRole, DeviceStatusRole };

    explicit DeviceModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    /* 在收到完整设备列表后原子替换数据，避免视图看到中间状态。 */
    void setDevices(const QList<ClientProtocol::DeviceInfo> &devices);
    quint64 deviceIdAt(int row) const;

private:
    QList<ClientProtocol::DeviceInfo> m_devices;
};

#endif // DEVICEMODEL_H
