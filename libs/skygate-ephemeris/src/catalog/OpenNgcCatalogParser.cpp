#include "catalog/OpenNgcCatalogParser.hpp"

#include "catalog/DelimitedCatalogReader.hpp"

#include <QByteArray>
#include <QRegularExpression>
#include <QString>
#include <QStringList>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace skygate::ephemeris {
namespace {

constexpr std::size_t kOpenNgcProgressCallbackInterval = 512;
constexpr std::size_t kMinOpenNgcRowCountLimit = 200000;
constexpr std::size_t kMinPlausibleOpenNgcRowBytes = 12;

std::string toStdString(const QString& text)
{
    const QByteArray utf8 = text.toUtf8();
    return std::string(utf8.constData(), static_cast<std::size_t>(utf8.size()));
}

std::optional<double> parseFiniteDouble(const QString& text)
{
    bool isDouble = false;
    const double value = text.trimmed().toDouble(&isDouble);
    if (!isDouble || !std::isfinite(value)) {
        return std::nullopt;
    }

    return value;
}

std::optional<double> parseRightAscensionHours(QString text)
{
    text = text.trimmed();
    if (text.isEmpty()) {
        return std::nullopt;
    }

    const QStringList parts = text.split(':');
    if (parts.size() != 3) {
        return parseFiniteDouble(text);
    }

    const auto hours = parseFiniteDouble(parts.at(0));
    const auto minutes = parseFiniteDouble(parts.at(1));
    const auto seconds = parseFiniteDouble(parts.at(2));
    if (!hours.has_value() || !minutes.has_value() || !seconds.has_value()) {
        return std::nullopt;
    }

    const double value = *hours + (*minutes / 60.0) + (*seconds / 3600.0);
    if (value < 0.0 || value >= 24.0) {
        return std::nullopt;
    }
    return value;
}

std::optional<double> parseDeclinationDeg(QString text)
{
    text = text.trimmed();
    if (text.isEmpty()) {
        return std::nullopt;
    }

    const QStringList parts = text.split(':');
    if (parts.size() != 3) {
        return parseFiniteDouble(text);
    }

    const double sign = text.startsWith('-') ? -1.0 : 1.0;
    QString degreesText = parts.at(0);
    degreesText.remove('+');
    degreesText.remove('-');
    const auto degrees = parseFiniteDouble(degreesText);
    const auto arcminutes = parseFiniteDouble(parts.at(1));
    const auto arcseconds = parseFiniteDouble(parts.at(2));
    if (!degrees.has_value() || !arcminutes.has_value() || !arcseconds.has_value()) {
        return std::nullopt;
    }

    const double value = sign * (*degrees + (*arcminutes / 60.0) + (*arcseconds / 3600.0));
    if (value < -90.0 || value > 90.0) {
        return std::nullopt;
    }
    return value;
}

std::optional<double> parsePositiveDouble(const QString& text)
{
    const auto value = parseFiniteDouble(text);
    if (!value.has_value() || *value <= 0.0) {
        return std::nullopt;
    }
    return value;
}

QString withoutLeadingZeros(const QString& value)
{
    QString trimmed = value.trimmed();
    while (trimmed.size() > 1 && trimmed.startsWith('0')) {
        trimmed.remove(0, 1);
    }
    return trimmed;
}

QString normalizedCatalogAlias(QString text)
{
    text = text.trimmed();
    if (text.isEmpty()) {
        return {};
    }

    text.replace('_', ' ');
    text.replace('-', ' ');
    text = text.simplified();

    const auto normalizePrefix = [&text](const QString& prefix) {
        if (!text.startsWith(prefix, Qt::CaseInsensitive)) {
            return;
        }

        QString suffix = text.sliced(prefix.size()).trimmed();
        suffix = withoutLeadingZeros(suffix);
        if (!suffix.isEmpty()) {
            text = QString("%1 %2").arg(prefix, suffix);
        }
    };

    normalizePrefix("NGC");
    normalizePrefix("IC");
    normalizePrefix("M");
    return text;
}

QString objectIdFromAlias(const QString& alias)
{
    const QString normalized = normalizedCatalogAlias(alias);
    if (normalized.startsWith("NGC ", Qt::CaseInsensitive)) {
        return "ngc_" + normalized.sliced(4).trimmed();
    }
    if (normalized.startsWith("IC ", Qt::CaseInsensitive)) {
        return "ic_" + normalized.sliced(3).trimmed();
    }
    if (normalized.startsWith("M ", Qt::CaseInsensitive)) {
        return "messier_" + normalized.sliced(2).trimmed().rightJustified(3, '0');
    }

    QString id = normalized.toLower();
    id.replace(QRegularExpression("[^a-z0-9]+"), "_");
    while (id.startsWith('_')) {
        id.remove(0, 1);
    }
    while (id.endsWith('_')) {
        id.chop(1);
    }
    return id.isEmpty() ? QString {} : "open_ngc_" + id;
}

void appendAlias(std::vector<std::string>& aliases, const QString& alias)
{
    const QString normalized = normalizedCatalogAlias(alias);
    if (normalized.isEmpty()) {
        return;
    }

    const std::string value = toStdString(normalized);
    const auto existingIt = std::find_if(
        aliases.begin(),
        aliases.end(),
        [&value](const std::string& existing) {
            return QString::fromStdString(existing).compare(
                QString::fromStdString(value),
                Qt::CaseInsensitive
            ) == 0;
        }
    );
    if (existingIt == aliases.end()) {
        aliases.push_back(value);
    }
}

void appendDelimitedAliases(std::vector<std::string>& aliases, const QString& text)
{
    for (const QString& token : text.split(',', Qt::SkipEmptyParts)) {
        appendAlias(aliases, token);
    }
}

DeepSkyObjectKind kindFromOpenNgcType(QString typeText)
{
    typeText = typeText.trimmed();
    if (typeText == "G" || typeText == "GGroup" || typeText == "GPair" || typeText == "GTrpl") {
        return DeepSkyObjectKind::Galaxy;
    }
    if (typeText == "OCl" || typeText == "Cl+N") {
        return DeepSkyObjectKind::OpenCluster;
    }
    if (typeText == "GCl") {
        return DeepSkyObjectKind::GlobularCluster;
    }
    if (typeText == "PN") {
        return DeepSkyObjectKind::PlanetaryNebula;
    }
    if (
        typeText == "Neb"
        || typeText == "HII"
        || typeText == "EmN"
        || typeText == "RfN"
        || typeText == "SNR"
    ) {
        return DeepSkyObjectKind::Nebula;
    }
    if (typeText == "*Ass") {
        return DeepSkyObjectKind::Asterism;
    }
    return DeepSkyObjectKind::Unknown;
}

bool shouldSkipOpenNgcType(const QString& typeText)
{
    return typeText == "NonEx" || typeText == "Dup" || typeText == "Star";
}

}  // namespace

CatalogBodyParseResult OpenNgcCatalogParser::parse(
    const std::string_view csvData,
    const HygParseProgressCallback& progressCallback
) const
{
    std::size_t parsedObjectCount = 0;

    CatalogBodyParseResult result = DelimitedCatalogReader::read(
        csvData,
        DelimitedCatalogReaderOptions {
            .separator = ';',
            .requiredColumns = {
                QStringLiteral("Name"),
                QStringLiteral("Type"),
                QStringLiteral("RA"),
                QStringLiteral("Dec"),
            },
            .invalidErrorCode = CatalogLoadErrorCode::InvalidOpenNgcCsv,
            .minRowCountLimit = kMinOpenNgcRowCountLimit,
            .minPlausibleRowBytes = kMinPlausibleOpenNgcRowBytes,
            .emptyInputDetail = "OpenNGC CSV payload is empty.",
            .missingColumnsDetail =
                "OpenNGC CSV payload is missing one of the required columns: "
                "Name, Type, RA, Dec.",
            .rowLimitDetail = "OpenNGC CSV payload exceeds the supported row limit.",
        },
        [&](const DelimitedCatalogReader::Row& row, CatalogBodyParseResult& rowResult) {
            const QString typeText = row.decodeColumn(QStringLiteral("Type"));
            if (shouldSkipOpenNgcType(typeText)) {
                return true;
            }

            const auto raHours = parseRightAscensionHours(row.decodeColumn(QStringLiteral("RA")));
            const auto decDeg = parseDeclinationDeg(row.decodeColumn(QStringLiteral("Dec")));
            if (!raHours.has_value() || !decDeg.has_value()) {
                return true;
            }

            std::vector<std::string> aliases;
            const QString name = row.decodeColumn(QStringLiteral("Name"));
            const QString messier = withoutLeadingZeros(row.decodeColumn(QStringLiteral("M")));
            const QString ngc = withoutLeadingZeros(row.decodeColumn(QStringLiteral("NGC")));
            const QString ic = withoutLeadingZeros(row.decodeColumn(QStringLiteral("IC")));
            if (!messier.isEmpty()) {
                appendAlias(aliases, "M " + messier);
            }
            if (!ngc.isEmpty()) {
                appendAlias(aliases, "NGC " + ngc);
            }
            if (!ic.isEmpty()) {
                appendAlias(aliases, "IC " + ic);
            }
            appendAlias(aliases, name);
            appendDelimitedAliases(aliases, row.decodeColumn(QStringLiteral("Identifiers")));
            appendDelimitedAliases(aliases, row.decodeColumn(QStringLiteral("Common names")));

            const QString displayName = !messier.isEmpty()
                ? QString("M%1").arg(messier)
                : (!ngc.isEmpty()
                    ? QString("NGC %1").arg(ngc)
                    : (!ic.isEmpty() ? QString("IC %1").arg(ic) : normalizedCatalogAlias(name)));
            const QString id = !messier.isEmpty()
                ? objectIdFromAlias("M " + messier)
                : (!ngc.isEmpty()
                    ? objectIdFromAlias("NGC " + ngc)
                    : (!ic.isEmpty() ? objectIdFromAlias("IC " + ic) : objectIdFromAlias(name)));
            if (displayName.isEmpty() || id.isEmpty()) {
                return true;
            }

            ++parsedObjectCount;
            if (progressCallback && (parsedObjectCount % kOpenNgcProgressCallbackInterval) == 0U) {
                progressCallback(parsedObjectCount);
            }

            const auto visualMagnitude = parseFiniteDouble(row.decodeColumn(QStringLiteral("V-Mag")));
            CelestialBody body;
            body.id = toStdString(id);
            body.displayName = toStdString(displayName);
            body.type = CelestialBodyType::DeepSkyObject;
            body.visualMagnitude = visualMagnitude.value_or(std::numeric_limits<double>::quiet_NaN());
            body.fixedEquatorial = core::EquatorialCoordinate {
                .rightAscensionHours = *raHours,
                .declinationDeg = *decDeg
            };
            body.deepSkyObject = DeepSkyObjectInfo {
                .kind = kindFromOpenNgcType(typeText),
                .aliases = std::move(aliases),
                .majorAxisArcmin = parsePositiveDouble(row.decodeColumn(QStringLiteral("MajAx"))),
                .minorAxisArcmin = parsePositiveDouble(row.decodeColumn(QStringLiteral("MinAx"))),
                .positionAngleDeg = parsePositiveDouble(row.decodeColumn(QStringLiteral("PosAng"))),
            };
            rowResult.bodies.push_back(std::move(body));
            return true;
        }
    );

    if (!result.isSuccess()) {
        return result;
    }

    if (progressCallback) {
        progressCallback(parsedObjectCount);
    }

    if (parsedObjectCount == 0U) {
        result.errorCode = CatalogLoadErrorCode::InvalidOpenNgcCsv;
        result.errorDetail = "OpenNGC CSV payload does not contain any valid deep-sky object rows.";
        return result;
    }

    result.diagnostics.parsedBodyCount = parsedObjectCount;
    return result;
}

}  // namespace skygate::ephemeris
