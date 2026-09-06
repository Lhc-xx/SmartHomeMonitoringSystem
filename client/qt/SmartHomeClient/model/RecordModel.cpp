#include "RecordModel.h"

/*
 * RecordModel 负责把录像查询结果转换为 Qt 的只读列表模型。
 * 模型只保存录像元数据，不读取文件、不解码媒体，避免把 C 成员负责的
 * FFmpeg/媒体管线耦合进客户端的数据层。
 */
RecordModel::RecordModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

int RecordModel::rowCount(const QModelIndex &parent) const
{
    /* 列表模型没有子节点；收到有效父索引时返回 0，符合 Qt 模型约定。 */
    return parent.isValid() ? 0 : m_records.size();
}

QVariant RecordModel::data(const QModelIndex &index, int role) const
{
    /* 先校验行号，防止界面刷新期间的旧索引访问越界。 */
    if (!index.isValid() || index.row() < 0 || index.row() >= m_records.size()) {
        return QVariant();
    }
    const ClientProtocol::RecordInfo &record = m_records.at(index.row());
    if (role == Qt::DisplayRole) {
        /* 默认展示路径和时间范围；其余字段通过自定义 role 提供给后续界面。 */
        return QStringLiteral("%1  (%2 - %3)")
            .arg(record.filePath, record.startTime, record.endTime);
    }
    if (role == RecordIdRole) return QVariant::fromValue(record.id);
    if (role == RecordDeviceIdRole) return QVariant::fromValue(record.deviceId);
    if (role == FilePathRole) return record.filePath;
    if (role == StartTimeRole) return record.startTime;
    if (role == EndTimeRole) return record.endTime;
    return QVariant();
}

QHash<int, QByteArray> RecordModel::roleNames() const
{
    /* 为 QML/自定义 delegate 提供稳定字段名，同时保留 Widgets 的 DisplayRole。 */
    QHash<int, QByteArray> roles;
    roles.insert(RecordIdRole, "recordId");
    roles.insert(RecordDeviceIdRole, "deviceId");
    roles.insert(FilePathRole, "filePath");
    roles.insert(StartTimeRole, "startTime");
    roles.insert(EndTimeRole, "endTime");
    return roles;
}

void RecordModel::setRecords(const QList<ClientProtocol::RecordInfo> &records)
{
    /* 批量替换结果并通知视图一次，避免逐条更新造成不必要的重绘。 */
    beginResetModel();
    m_records = records;
    endResetModel();
}
