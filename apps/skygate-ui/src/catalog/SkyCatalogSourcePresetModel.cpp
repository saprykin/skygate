#include "SkyCatalogSourcePresetModel.hpp"

#include <utility>

namespace skygate::ui::internal {

SkyCatalogSourcePresetModel::SkyCatalogSourcePresetModel(
    QVector<SkyCatalogSourceDescriptor> descriptors, const bool includeCustomOption, QObject* parent
)
    : SkyCatalogSourcePresetModel(
          std::move(descriptors),
          includeCustomOption ? QVector<CustomOption>{{QStringLiteral("Custom URL"), QString()}}
                              : QVector<CustomOption>{},
          parent
      )
{
}

SkyCatalogSourcePresetModel::SkyCatalogSourcePresetModel(
    QVector<SkyCatalogSourceDescriptor> descriptors, QVector<CustomOption> customOptions, QObject* parent
)
    : QAbstractListModel(parent)
{
    m_rows.reserve(descriptors.size() + customOptions.size());
    for (SkyCatalogSourceDescriptor& descriptor : descriptors) {
        Row row;
        row.descriptor = std::move(descriptor);
        row.category = row.descriptor.category;
        m_rows.push_back(std::move(row));
    }

    for (const CustomOption& option : customOptions) {
        Row row;
        row.custom = true;
        row.descriptor.title = option.title;
        row.descriptor.legacyPresetIndex = 2;
        row.category = option.category;
        m_rows.push_back(std::move(row));
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
    case CategoryRole:
        return row->category;
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
        {CategoryRole, "category"},
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

QString SkyCatalogSourcePresetModel::categoryAt(const int row) const
{
    const Row* entry = rowAt(row);
    return entry == nullptr ? QString() : entry->category;
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
