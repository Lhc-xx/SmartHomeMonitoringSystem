#ifndef RECORDMODEL_H
#define RECORDMODEL_H

#include <QAbstractListModel>

#include "protocol/ClientProtocol.h"

/*
 * RecordModel 只显示录像元数据中的路径与时间范围。
 * 禁止在此模型中引入 FFmpeg、解码器或任何视频播放职责。
 */
class RecordModel : public QAbstractListModel
{
    Q_OBJECT

public:
    enum RecordRole { RecordIdRole = Qt::UserRole + 1, RecordDeviceIdRole,
                      FilePathRole, StartTimeRole, EndTimeRole };

    explicit RecordModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    /* 将服务端成功返回的录像元数据一次性更新到视图。 */
    void setRecords(const QList<ClientProtocol::RecordInfo> &records);

private:
    QList<ClientProtocol::RecordInfo> m_records;
};

#endif // RECORDMODEL_H
