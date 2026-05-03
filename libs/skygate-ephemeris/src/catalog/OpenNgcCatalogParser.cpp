#include "catalog/OpenNgcCatalogParser.hpp"

#include "catalog/CatalogParsingUtilities.hpp"
#include "catalog/DelimitedCatalogReader.hpp"
#include "catalog/OpenNgcObjectMapper.hpp"

#include <QString>

#include <cstddef>
#include <limits>
#include <utility>

namespace skygate::ephemeris {
namespace {

constexpr std::size_t kOpenNgcProgressCallbackInterval = 512;
constexpr std::size_t kMinOpenNgcRowCountLimit = 200000;
constexpr std::size_t kMinPlausibleOpenNgcRowBytes = 12;

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
