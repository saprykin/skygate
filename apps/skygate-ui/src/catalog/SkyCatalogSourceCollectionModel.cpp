#include "SkyCatalogSourceCollectionModel.hpp"

#include <utility>

namespace skygate::ui::internal {

SkyCatalogSourceCollectionModel::SkyCatalogSourceCollectionModel(QObject* parent) : QAbstractListModel(parent) {}

int SkyCatalogSourceCollectionModel::rowCount(const QModelIndex& parent) const
{
    if (parent.isValid()) {
        return 0;
    }

    return m_entries.size();
}

QVariant SkyCatalogSourceCollectionModel::data(const QModelIndex& index, const int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_entries.size()) {
        return {};
    }

    const SourceEntry& entry = m_entries.at(index.row());
    switch (role) {
    case Qt::DisplayRole:
    case TitleRole:
        return entry.title;
    case InstanceIdRole:
        return entry.instanceId;
    case VersionRole:
        return entry.version;
    case CategoryRole:
        return entry.category;
    case EnabledRole:
        return entry.enabled;
    case BundledRole:
        return entry.bundled;
    case BusyRole:
        return entry.busy;
    case HasErrorRole:
        return entry.hasError;
    case StatusRole:
        return entry.statusText;
    case ObjectCountRole:
        return entry.objectCount;
    default:
        return {};
    }
}

QHash<int, QByteArray> SkyCatalogSourceCollectionModel::roleNames() const
{
    return {
        {InstanceIdRole, "instanceId"},
        {TitleRole, "title"},
        {VersionRole, "version"},
        {CategoryRole, "category"},
        {EnabledRole, "sourceEnabled"},
        {BundledRole, "bundled"},
        {BusyRole, "busy"},
        {HasErrorRole, "hasError"},
        {StatusRole, "status"},
        {ObjectCountRole, "objectCount"},
    };
}

void SkyCatalogSourceCollectionModel::replaceEntries(QVector<SourceEntry> entries)
{
    beginResetModel();
    m_entries = std::move(entries);
    endResetModel();
}

int SkyCatalogSourceCollectionModel::indexOfInstanceId(const QString& instanceId) const
{
    for (int index = 0; index < m_entries.size(); ++index) {
        if (m_entries.at(index).instanceId == instanceId) {
            return index;
        }
    }
    return -1;
}

}  // namespace skygate::ui::internal
