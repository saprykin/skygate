#include "catalog/opengc/OpenNgcCatalogParser.hpp"

#include "catalog/normalize/CatalogParsingUtilities.hpp"
#include "catalog/io/DelimitedCatalogReader.hpp"
#include "catalog/opengc/OpenNgcObjectMapper.hpp"

#include <QLoggingCategory>
#include <QString>
#include <QStringList>

#include <cstddef>
#include <limits>
#include <utility>

namespace skygate::ephemeris {
namespace {

constexpr std::size_t kOpenNgcProgressCallbackInterval = 512;
constexpr std::size_t kMinOpenNgcRowCountLimit = 200000;
constexpr std::size_t kMinPlausibleOpenNgcRowBytes = 12;
constexpr std::size_t kMaxInvalidRowSamples = 5;

Q_LOGGING_CATEGORY(skygateCatalogParseLog, "skygate.catalog.parse")

}  // namespace

CatalogBodyParseResult OpenNgcCatalogParser::parse(
    const std::string_view csvData,
    const HygParseProgressCallback& progressCallback
) const
{
    std::size_t parsedObjectCount = 0;
    std::size_t dataRowNumber = 0;
    std::size_t invalidCoordinateRowCount = 0;
    std::size_t invalidMappingRowCount = 0;
    QStringList invalidCoordinateRowSamples;
    QStringList invalidMappingRowSamples;

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
            ++dataRowNumber;
            const QString typeText = row.decodeColumn(QStringLiteral("Type"));
            if (OpenNgcObjectMapper::shouldSkipType(typeText)) {
                return true;
            }

            const auto raHours = catalog_parsing::parseRightAscensionHours(
                row.decodeColumn(QStringLiteral("RA"))
            );
            const auto decDeg = catalog_parsing::parseDeclinationDeg(
                row.decodeColumn(QStringLiteral("Dec"))
            );
            if (!raHours.has_value() || !decDeg.has_value()) {
                ++invalidCoordinateRowCount;
                if (invalidCoordinateRowSamples.size() < static_cast<qsizetype>(kMaxInvalidRowSamples)) {
                    invalidCoordinateRowSamples.push_back(QStringLiteral(
                        "row %1 name='%2' ra='%3' dec='%4'"
                    ).arg(
                        QString::number(static_cast<qulonglong>(dataRowNumber + 1U)),
                        row.decodeColumn(QStringLiteral("Name")),
                        row.decodeColumn(QStringLiteral("RA")),
                        row.decodeColumn(QStringLiteral("Dec"))
                    ));
                }
                return true;
            }

            const QString name = row.decodeColumn(QStringLiteral("Name"));
            const QString messier = OpenNgcObjectMapper::withoutLeadingZeros(
                row.decodeColumn(QStringLiteral("M"))
            );
            const QString ngc = OpenNgcObjectMapper::withoutLeadingZeros(
                row.decodeColumn(QStringLiteral("NGC"))
            );
            const QString ic = OpenNgcObjectMapper::withoutLeadingZeros(
                row.decodeColumn(QStringLiteral("IC"))
            );
            auto mapping = OpenNgcObjectMapper::mapObject(
                typeText,
                name,
                messier,
                ngc,
                ic,
                row.decodeColumn(QStringLiteral("Identifiers")),
                row.decodeColumn(QStringLiteral("Common names"))
            );
            if (mapping.displayName.empty() || mapping.id.empty()) {
                ++invalidMappingRowCount;
                if (invalidMappingRowSamples.size() < static_cast<qsizetype>(kMaxInvalidRowSamples)) {
                    invalidMappingRowSamples.push_back(QStringLiteral(
                        "row %1 name='%2' type='%3'"
                    ).arg(
                        QString::number(static_cast<qulonglong>(dataRowNumber + 1U)),
                        name,
                        typeText
                    ));
                }
                return true;
            }

            ++parsedObjectCount;
            if (progressCallback && (parsedObjectCount % kOpenNgcProgressCallbackInterval) == 0U) {
                progressCallback(parsedObjectCount);
            }

            const auto visualMagnitude = catalog_parsing::parseFiniteDouble(
                row.decodeColumn(QStringLiteral("V-Mag"))
            );
            CelestialBody body;
            body.id = std::move(mapping.id);
            body.displayName = std::move(mapping.displayName);
            body.type = CelestialBodyType::DeepSkyObject;
            body.visualMagnitude = visualMagnitude.value_or(
                std::numeric_limits<double>::quiet_NaN()
            );
            body.fixedEquatorial = core::EquatorialCoordinate {
                .rightAscensionHours = *raHours,
                .declinationDeg = *decDeg
            };
            body.deepSkyObject = DeepSkyObjectInfo {
                .kind = mapping.kind,
                .aliases = std::move(mapping.aliases),
                .majorAxisArcmin =
                    catalog_parsing::parsePositiveDouble(row.decodeColumn(QStringLiteral("MajAx"))),
                .minorAxisArcmin =
                    catalog_parsing::parsePositiveDouble(row.decodeColumn(QStringLiteral("MinAx"))),
                .positionAngleDeg =
                    catalog_parsing::parseNonNegativeDouble(
                        row.decodeColumn(QStringLiteral("PosAng"))
                    ),
            };
            rowResult.bodies.push_back(std::move(body));
            return true;
        }
    );

    if (!result.isSuccess()) {
        qCWarning(skygateCatalogParseLog).noquote()
            << "OpenNGC CSV parse failed:" << QString::fromStdString(result.errorDetail);
        return result;
    }

    if (invalidCoordinateRowCount > 0U) {
        qCWarning(skygateCatalogParseLog).noquote()
            << "OpenNGC CSV skipped" << static_cast<qulonglong>(invalidCoordinateRowCount)
            << "rows with invalid coordinates; samples:"
            << invalidCoordinateRowSamples.join(QStringLiteral("; "));
    }
    if (invalidMappingRowCount > 0U) {
        qCWarning(skygateCatalogParseLog).noquote()
            << "OpenNGC CSV skipped" << static_cast<qulonglong>(invalidMappingRowCount)
            << "rows with unmappable identifiers; samples:"
            << invalidMappingRowSamples.join(QStringLiteral("; "));
    }

    if (progressCallback) {
        progressCallback(parsedObjectCount);
    }

    if (parsedObjectCount == 0U) {
        result.errorCode = CatalogLoadErrorCode::InvalidOpenNgcCsv;
        result.errorDetail = "OpenNGC CSV payload does not contain any valid deep-sky object rows.";
        qCWarning(skygateCatalogParseLog).noquote()
            << "OpenNGC CSV parse failed:" << QString::fromStdString(result.errorDetail);
        return result;
    }

    result.diagnostics.parsedBodyCount = parsedObjectCount;
    return result;
}

}  // namespace skygate::ephemeris
