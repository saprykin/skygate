#include "catalog/opengc/OpenNgcCatalogParser.hpp"
#include "catalog/normalize/CatalogParsingUtilities.hpp"
#include "catalog/io/DelimitedCatalogParser.hpp"
#include "catalog/opengc/OpenNgcObjectMapper.hpp"

#include <QString>

#include <cstddef>
#include <limits>
#include <string_view>
#include <utility>

namespace skygate::ephemeris {
namespace {

constexpr std::size_t kOpenNgcRowCountLimitFloor = 200000;
constexpr std::size_t kOpenNgcMinExpectedBytesPerDataRow = 12;

constexpr std::string_view kOpenNgcInvalidCategoryLabels[] = {
    "invalid coordinates",
    "unmappable identifiers",
};

}  // namespace

CatalogBodyParseResult
OpenNgcCatalogParser::parse(const std::string_view csvData, const HygParseProgressCallback& progressCallback) const
{
    return DelimitedCatalogParser::run(
        csvData,
        DelimitedCatalogReaderOptions{
            .separator = ';',
            .requiredColumns =
                {
                    QStringLiteral("Name"),
                    QStringLiteral("Type"),
                    QStringLiteral("RA"),
                    QStringLiteral("Dec"),
                },
            .invalidErrorCode = CatalogLoadResult::ErrorCode::InvalidOpenNgcCsv,
            .rowCountLimitFloor = kOpenNgcRowCountLimitFloor,
            .minExpectedBytesPerDataRow = kOpenNgcMinExpectedBytesPerDataRow,
            .emptyInputDetail = "OpenNGC CSV payload is empty.",
            .missingColumnsDetail = "OpenNGC CSV payload is missing one of the required columns: "
                                    "Name, Type, RA, Dec.",
            .rowLimitDetail = "OpenNGC CSV payload exceeds the supported row limit.",
        },
        DelimitedCatalogParserOptions{
            .zeroResultCode = CatalogLoadResult::ErrorCode::InvalidOpenNgcCsv,
            .zeroResultDetail = "OpenNGC CSV payload does not contain any valid deep-sky object rows.",
            .formatName = "OpenNGC CSV",
        },
        kOpenNgcInvalidCategoryLabels,
        [](const DelimitedCatalogRow& row, std::size_t rowNumber) -> RowParseOutcome {
            const QString typeText = row.decodeColumn(QStringLiteral("Type"));
            if (OpenNgcObjectMapper::shouldSkipType(typeText)) {
                return {};
            }

            const auto raHours =
                CatalogParsingUtilities::parseRightAscensionHours(row.decodeColumn(QStringLiteral("RA")));
            const auto decDeg = CatalogParsingUtilities::parseDeclinationDeg(row.decodeColumn(QStringLiteral("Dec")));
            if (!raHours.has_value() || !decDeg.has_value()) {
                return RowParseOutcome{
                    .invalidCategoryIndex = 0,
                    .invalidSample = QStringLiteral("row %1 name='%2' ra='%3' dec='%4'")
                                         .arg(
                                             QString::number(static_cast<qulonglong>(rowNumber + 1U)),
                                             row.decodeColumn(QStringLiteral("Name")),
                                             row.decodeColumn(QStringLiteral("RA")),
                                             row.decodeColumn(QStringLiteral("Dec"))
                                         ),
                };
            }

            const QString name = row.decodeColumn(QStringLiteral("Name"));
            const QString messier = OpenNgcObjectMapper::withoutLeadingZeros(row.decodeColumn(QStringLiteral("M")));
            const QString ngc = OpenNgcObjectMapper::withoutLeadingZeros(row.decodeColumn(QStringLiteral("NGC")));
            const QString ic = OpenNgcObjectMapper::withoutLeadingZeros(row.decodeColumn(QStringLiteral("IC")));
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
                return RowParseOutcome{
                    .invalidCategoryIndex = 1,
                    .invalidSample = QStringLiteral("row %1 name='%2' type='%3'")
                                         .arg(QString::number(static_cast<qulonglong>(rowNumber + 1U)), name, typeText),
                };
            }

            const auto visualMagnitude =
                CatalogParsingUtilities::parseFiniteDouble(row.decodeColumn(QStringLiteral("V-Mag")));
            CelestialBody body;
            body.id = std::move(mapping.id);
            body.displayName = std::move(mapping.displayName);
            body.type = CelestialBodyType::DeepSkyObject;
            body.visualMagnitude = visualMagnitude.value_or(std::numeric_limits<double>::quiet_NaN());
            body.fixedEquatorial =
                core::EquatorialCoordinate{.rightAscensionHours = *raHours, .declinationDeg = *decDeg};
            body.deepSkyObject = DeepSkyObjectInfo{
                .kind = mapping.kind,
                .aliases = std::move(mapping.aliases),
                .majorAxisArcmin =
                    CatalogParsingUtilities::parsePositiveDouble(row.decodeColumn(QStringLiteral("MajAx"))),
                .minorAxisArcmin =
                    CatalogParsingUtilities::parsePositiveDouble(row.decodeColumn(QStringLiteral("MinAx"))),
                .positionAngleDeg =
                    CatalogParsingUtilities::parseNonNegativeDouble(row.decodeColumn(QStringLiteral("PosAng"))),
            };

            return RowParseOutcome{.body = std::move(body)};
        },
        progressCallback
    );
}

}  // namespace skygate::ephemeris
