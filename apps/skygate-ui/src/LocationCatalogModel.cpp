#include "LocationCatalogModel.hpp"

#include <QFile>
#include <QIODevice>
#include <QResource>
#include <QTextStream>

#include <algorithm>

namespace {

constexpr auto kLocationCatalogResourcePath = ":/locations/major-cities.csv";

QString normalizedFilterText(const QString& value)
{
    return value.trimmed().toCaseFolded();
}

bool matchesFilter(const LocationCatalogEntry& entry, const QString& filterText)
{
    if (filterText.isEmpty()) {
        return true;
    }

    return entry.cityName.toCaseFolded().contains(filterText)
        || entry.countryName.toCaseFolded().contains(filterText);
}

}  // namespace

static void ensureLocationCatalogResourcesLoaded()
{
    Q_INIT_RESOURCE(locations);
}

QString LocationCatalogEntry::displayText() const
{
    return QString("%1, %2").arg(cityName, countryName);
}

LocationCatalogModel::LocationCatalogModel(QObject* parent)
    : QAbstractListModel(parent)
{
    ensureLocationCatalogResourcesLoaded();
    (void)loadCatalog();
    rebuildRows();
}

int LocationCatalogModel::rowCount(const QModelIndex& parent) const
{
    if (parent.isValid()) {
        return 0;
    }

    return m_rows.size();
}

QVariant LocationCatalogModel::data(const QModelIndex& index, const int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_rows.size()) {
        return {};
    }

    const Row& row = m_rows.at(index.row());
    switch (role) {
    case RowKindRole:
        return row.kind == RowKind::CountryHeader ? "countryHeader" : "city";
    case DisplayTextRole:
        return row.displayText;
    case DetailTextRole:
        return row.detailText;
    case CityIdRole:
        return row.kind == RowKind::City ? row.entry.id : QString();
    case CountryNameRole:
        return row.kind == RowKind::City ? row.entry.countryName : row.displayText;
    case CityNameRole:
        return row.kind == RowKind::City ? row.entry.cityName : QString();
    case LatitudeDegRole:
        return row.kind == RowKind::City ? QVariant(row.entry.latitudeDeg) : QVariant();
    case LongitudeDegRole:
        return row.kind == RowKind::City ? QVariant(row.entry.longitudeDeg) : QVariant();
    case SelectableRole:
        return row.kind == RowKind::City;
    default:
        return {};
    }
}

QHash<int, QByteArray> LocationCatalogModel::roleNames() const
{
    return {
        {RowKindRole, "rowKind"},
        {DisplayTextRole, "displayText"},
        {DetailTextRole, "detailText"},
        {CityIdRole, "cityId"},
        {CountryNameRole, "countryName"},
        {CityNameRole, "cityName"},
        {LatitudeDegRole, "latitudeDeg"},
        {LongitudeDegRole, "longitudeDeg"},
        {SelectableRole, "selectable"},
    };
}

QString LocationCatalogModel::filterText() const
{
    return m_filterText;
}

void LocationCatalogModel::setFilterText(const QString& filterText)
{
    const QString normalized = normalizedFilterText(filterText);
    if (m_filterText == normalized) {
        return;
    }

    m_filterText = normalized;
    rebuildRows();
    emit filterTextChanged();
}

bool LocationCatalogModel::hasCityId(const QString& cityId) const
{
    return entryForCityId(cityId).has_value();
}

std::optional<LocationCatalogEntry> LocationCatalogModel::entryForCityId(
    const QString& cityId
) const
{
    if (cityId.trimmed().isEmpty()) {
        return std::nullopt;
    }

    const auto entryIterator = std::find_if(
        m_entries.cbegin(),
        m_entries.cend(),
        [&cityId](const LocationCatalogEntry& entry) {
            return entry.id == cityId;
        }
    );
    if (entryIterator == m_entries.cend()) {
        return std::nullopt;
    }

    return *entryIterator;
}

bool LocationCatalogModel::loadCatalog()
{
    QFile catalogFile(QString::fromUtf8(kLocationCatalogResourcePath));
    if (!catalogFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return false;
    }

    QTextStream stream(&catalogFile);
    if (stream.atEnd()) {
        return false;
    }

    const QStringList headers = stream.readLine().split(',');
    const int idIndex = headers.indexOf("id");
    const int countryNameIndex = headers.indexOf("countryName");
    const int cityNameIndex = headers.indexOf("cityName");
    const int latitudeIndex = headers.indexOf("latitudeDeg");
    const int longitudeIndex = headers.indexOf("longitudeDeg");
    if (
        idIndex < 0
        || countryNameIndex < 0
        || cityNameIndex < 0
        || latitudeIndex < 0
        || longitudeIndex < 0
    ) {
        return false;
    }

    QVector<LocationCatalogEntry> entries;
    while (!stream.atEnd()) {
        const QString line = stream.readLine().trimmed();
        if (line.isEmpty()) {
            continue;
        }

        const QStringList fields = line.split(',');
        if (fields.size() != headers.size()) {
            continue;
        }

        bool latitudeIsValid = false;
        bool longitudeIsValid = false;
        LocationCatalogEntry entry;
        entry.id = fields.at(idIndex).trimmed();
        entry.countryName = fields.at(countryNameIndex).trimmed();
        entry.cityName = fields.at(cityNameIndex).trimmed();
        entry.latitudeDeg = fields.at(latitudeIndex).trimmed().toDouble(&latitudeIsValid);
        entry.longitudeDeg = fields.at(longitudeIndex).trimmed().toDouble(&longitudeIsValid);
        if (
            entry.id.isEmpty()
            || entry.countryName.isEmpty()
            || entry.cityName.isEmpty()
            || !latitudeIsValid
            || !longitudeIsValid
        ) {
            continue;
        }

        entries.push_back(entry);
    }

    std::sort(entries.begin(), entries.end(), [](const LocationCatalogEntry& lhs, const LocationCatalogEntry& rhs) {
        const int countryCompare = QString::compare(
            lhs.countryName,
            rhs.countryName,
            Qt::CaseInsensitive
        );
        if (countryCompare != 0) {
            return countryCompare < 0;
        }

        return QString::compare(lhs.cityName, rhs.cityName, Qt::CaseInsensitive) < 0;
    });

    m_entries = std::move(entries);
    return !m_entries.isEmpty();
}

void LocationCatalogModel::rebuildRows()
{
    beginResetModel();
    m_rows.clear();

    QString currentCountryName;
    for (const LocationCatalogEntry& entry : m_entries) {
        if (!matchesFilter(entry, m_filterText)) {
            continue;
        }

        if (currentCountryName != entry.countryName) {
            currentCountryName = entry.countryName;
            m_rows.push_back(Row {
                .kind = RowKind::CountryHeader,
                .displayText = currentCountryName,
            });
        }

        m_rows.push_back(Row {
            .kind = RowKind::City,
            .displayText = entry.cityName,
            .detailText = entry.countryName,
            .entry = entry,
        });
    }

    endResetModel();
}
