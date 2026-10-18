#include "LocationCatalogModel.hpp"
#include "TextParsingUtilities.hpp"
#include "catalog/io/CatalogParsingUtilities.hpp"

#include <QFile>
#include <QIODevice>
#include <QLoggingCategory>
#include <QResource>
#include <QStringList>
#include <QTextStream>

#include <algorithm>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace {

Q_LOGGING_CATEGORY(skygateLocationCatalogLog, "skygate.location.catalog")

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

    return entry.cityName.toCaseFolded().contains(filterText) || entry.countryName.toCaseFolded().contains(filterText);
}

[[nodiscard]] QStringList splitCatalogFields(const QString& line)
{
    const std::string lineText = skygate::ephemeris::CatalogParsingUtilities::toUtf8String(line);
    QStringList fields;
    for (const std::string_view field : skygate::ephemeris::TextParsingUtilities::splitCommaSeparated(lineText)) {
        fields.push_back(QString::fromUtf8(field.data(), static_cast<qsizetype>(field.size())));
    }
    return fields;
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

LocationCatalogModel::LocationCatalogModel(QObject* parent) : QAbstractListModel(parent)
{
    ensureLocationCatalogResourcesLoaded();
    if (!loadCatalog()) {
        qCWarning(skygateLocationCatalogLog) << "Failed to load the bundled location catalog.";
    }
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

std::optional<LocationCatalogEntry> LocationCatalogModel::entryForCityId(const QString& cityId) const
{
    if (cityId.trimmed().isEmpty()) {
        return std::nullopt;
    }

    const auto entryIterator =
        std::find_if(m_entries.cbegin(), m_entries.cend(), [&cityId](const LocationCatalogEntry& entry) {
            return entry.id == cityId;
        });
    if (entryIterator == m_entries.cend()) {
        return std::nullopt;
    }

    return *entryIterator;
}

bool LocationCatalogModel::loadCatalog()
{
    QFile catalogFile(QString::fromUtf8(kLocationCatalogResourcePath));
    if (!catalogFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qCWarning(skygateLocationCatalogLog).noquote()
            << QStringLiteral("Unable to open the location catalog resource %1.")
                   .arg(QString::fromUtf8(kLocationCatalogResourcePath));
        return false;
    }

    QTextStream stream(&catalogFile);
    const std::optional<QVector<LocationCatalogEntry>> parsedEntries = parseCatalog(stream.readAll());
    if (!parsedEntries.has_value()) {
        return false;
    }
    if (parsedEntries->isEmpty()) {
        qCWarning(skygateLocationCatalogLog) << "The location catalog has no usable entries.";
        return false;
    }

    m_entries = *parsedEntries;
    return true;
}

std::optional<QVector<LocationCatalogEntry>> LocationCatalogModel::parseCatalog(const QString& csvText)
{
    const QStringList lines = csvText.split(QLatin1Char('\n'));
    const QStringList headers = splitCatalogFields(lines.constFirst());
    const int idIndex = headers.indexOf(QStringLiteral("id"));
    const int countryNameIndex = headers.indexOf(QStringLiteral("countryName"));
    const int cityNameIndex = headers.indexOf(QStringLiteral("cityName"));
    const int latitudeIndex = headers.indexOf(QStringLiteral("latitudeDeg"));
    const int longitudeIndex = headers.indexOf(QStringLiteral("longitudeDeg"));
    if (idIndex < 0 || countryNameIndex < 0 || cityNameIndex < 0 || latitudeIndex < 0 || longitudeIndex < 0) {
        qCWarning(skygateLocationCatalogLog) << "The location catalog header is missing required columns.";
        return std::nullopt;
    }

    QVector<LocationCatalogEntry> entries;
    for (qsizetype lineIndex = 1; lineIndex < lines.size(); ++lineIndex) {
        const QString line = lines.at(lineIndex).trimmed();
        if (line.isEmpty()) {
            continue;
        }

        const QStringList fields = splitCatalogFields(line);
        if (fields.size() != headers.size()) {
            qCWarning(skygateLocationCatalogLog).noquote()
                << QStringLiteral("Skipping location catalog line %1 with %2 fields instead of %3.")
                       .arg(lineIndex + 1)
                       .arg(fields.size())
                       .arg(headers.size());
            continue;
        }

        const QString cityId = fields.at(idIndex);
        const QString countryName = fields.at(countryNameIndex);
        const QString cityName = fields.at(cityNameIndex);
        const std::optional<double> latitudeDeg =
            skygate::ephemeris::CatalogParsingUtilities::parseFiniteDouble(fields.at(latitudeIndex));
        const std::optional<double> longitudeDeg =
            skygate::ephemeris::CatalogParsingUtilities::parseFiniteDouble(fields.at(longitudeIndex));
        if (cityId.isEmpty() || countryName.isEmpty() || cityName.isEmpty()) {
            qCWarning(skygateLocationCatalogLog).noquote()
                << QStringLiteral("Skipping location catalog line %1 with missing values.").arg(lineIndex + 1);
            continue;
        }
        if (!latitudeDeg.has_value() || !longitudeDeg.has_value()) {
            qCWarning(skygateLocationCatalogLog).noquote()
                << QStringLiteral("Skipping location catalog line %1 with non-finite coordinates.").arg(lineIndex + 1);
            continue;
        }

        entries.push_back(
            LocationCatalogEntry{
                .id = cityId,
                .countryName = countryName,
                .cityName = cityName,
                .latitudeDeg = *latitudeDeg,
                .longitudeDeg = *longitudeDeg,
            }
        );
    }

    std::sort(entries.begin(), entries.end(), [](const LocationCatalogEntry& lhs, const LocationCatalogEntry& rhs) {
        const int countryCompare = QString::compare(lhs.countryName, rhs.countryName, Qt::CaseInsensitive);
        if (countryCompare != 0) {
            return countryCompare < 0;
        }

        return QString::compare(lhs.cityName, rhs.cityName, Qt::CaseInsensitive) < 0;
    });

    return entries;
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
            m_rows.push_back(
                Row{
                    .kind = RowKind::CountryHeader,
                    .displayText = currentCountryName,
                }
            );
        }

        m_rows.push_back(
            Row{
                .kind = RowKind::City,
                .displayText = entry.cityName,
                .detailText = entry.countryName,
                .entry = entry,
            }
        );
    }

    endResetModel();
}
