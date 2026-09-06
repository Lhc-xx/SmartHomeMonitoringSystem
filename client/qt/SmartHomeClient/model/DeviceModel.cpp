#include "DeviceModel.h"

DeviceModel::DeviceModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

int DeviceModel::rowCount(const QModelIndex &parent) const
{
    /* 列表模型不支持树形子节点；传入子节点索引时必须返回零行。 */
    return parent.isValid() ? 0 : m_devices.size();
}

QVariant DeviceModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_devices.size()) {
        return QVariant();
    }
    const ClientProtocol::DeviceInfo &device = m_devices.at(index.row());
    if (role == Qt::DisplayRole) {
        /* 默认显示文本保持紧凑，同时让状态在没有自定义 delegate 时也可见。 */
        return QStringLiteral("%1  [%2]").arg(device.name, device.status);
    }
    if (role == DeviceIdRole) return QVariant::fromValue(device.id);
    if (role == DeviceNameRole) return device.name;
    if (role == DeviceTypeRole) return device.type;
    if (role == DeviceStatusRole) return device.status;
    return QVariant();
}

QHash<int, QByteArray> DeviceModel::roleNames() const
{
    QHash<int, QByteArray> roles;
    roles.insert(DeviceIdRole, "deviceId");
    roles.insert(DeviceNameRole, "name");
    roles.insert(DeviceTypeRole, "type");
    roles.insert(DeviceStatusRole, "status");
    return roles;
}

void DeviceModel::setDevices(const QList<ClientProtocol::DeviceInfo> &devices)
{
    beginResetModel();
    m_devices = devices;
    endResetModel();
}

quint64 DeviceModel::deviceIdAt(int row) const
{
    return row >= 0 && row < m_devices.size() ? m_devices.at(row).id : 0;
}
