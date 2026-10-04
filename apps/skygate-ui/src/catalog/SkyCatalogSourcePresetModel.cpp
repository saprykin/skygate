#include "SkyCatalogSourcePresetModel.hpp"

#include <utility>

namespace skygate::ui::internal {

SkyCatalogSourcePresetModel::SkyCatalogSourcePresetModel(
    QVector<SkyCatalogSourceDescriptor> descriptors, const bool includeCustomOption, QObject* parent
)
    : QAbstractListModel(parent)
{
    m_rows.reserve(descriptors.size() + (includeCustomOption ? 1 : 0));
    for (SkyCatalogSourceDescriptor& descriptor : descriptors) {
        m_rows.push_back(Row{.descriptor = std::move(descriptor)});
    }

    if (includeCustomOption) {
        Row customRow;
        customRow.custom = true;
        customRow.descriptor.title = QStringLiteral("Custom URL");
        customRow.descriptor.legacyPresetIndex = 2;
        m_rows.push_back(std::move(customRow));
    }
}

int SkyCatalogSourcePresetModel::rowCount(const QModelIndex& parent) const
{
    if (parent.isValid()) {
        return 0;
    }

    return m_rows.size();
}

QVariant SkyCatalogSourcePresetModel::data(const QModelIndex& index, const int role) const
{
    if (!index.isValid()) {
        return {};
    }

    const Row* row = rowAt(index.row());
    if (row == nullptr) {
        return {};
    }

    switch (role) {
    case Qt::DisplayRole:
    case TitleRole:
        return row->descriptor.title;
    case SourceIdRole:
        return row->custom ? QString() : row->descriptor.sourceId;
    case VersionRole:
        return row->descriptor.version;
    case DefaultUrlRole:
        return row->custom ? QString() : row->descriptor.defaultUrl();
    case AttributionRole:
        return row->descriptor.attribution;
    case LegacyPresetIndexRole:
        return row->custom ? 2 : row->descriptor.legacyPresetIndex;
    case BundledRole:
        return !row->custom && row->descriptor.bundled;
    case CustomRole:
        return row->custom;
    default:
        return {};
    }
}

QHash<int, QByteArray> SkyCatalogSourcePresetModel::roleNames() const
{
    return {
        {TitleRole, "title"},
        {SourceIdRole, "sourceId"},
        {VersionRole, "version"},
        {DefaultUrlRole, "defaultUrl"},
        {AttributionRole, "attribution"},
        {LegacyPresetIndexRole, "legacyPresetIndex"},
        {BundledRole, "bundled"},
        {CustomRole, "custom"},
    };
}

QString SkyCatalogSourcePresetModel::sourceIdAt(const int row) const
{
    const Row* entry = rowAt(row);
    return entry == nullptr || entry->custom ? QString() : entry->descriptor.sourceId;
}

QString SkyCatalogSourcePresetModel::titleAt(const int row) const
{
    const Row* entry = rowAt(row);
    return entry == nullptr ? QString() : entry->descriptor.title;
}

QString SkyCatalogSourcePresetModel::defaultUrlAt(const int row) const
{
    const Row* entry = rowAt(row);
    return entry == nullptr || entry->custom ? QString() : entry->descriptor.defaultUrl();
}

QString SkyCatalogSourcePresetModel::versionAt(const int row) const
{
    const Row* entry = rowAt(row);
    return entry == nullptr ? QString() : entry->descriptor.version;
}

int SkyCatalogSourcePresetModel::legacyPresetIndexAt(const int row) const
{
    const Row* entry = rowAt(row);
    return entry == nullptr ? 0 : (entry->custom ? 2 : entry->descriptor.legacyPresetIndex);
}

bool SkyCatalogSourcePresetModel::isCustomAt(const int row) const
{
    const Row* entry = rowAt(row);
    return entry != nullptr && entry->custom;
}

const SkyCatalogSourcePresetModel::Row* SkyCatalogSourcePresetModel::rowAt(const int row) const
{
    if (row < 0 || row >= m_rows.size()) {
        return nullptr;
    }

    return &m_rows.at(row);
}

}  // namespace skygate::ui::internal
