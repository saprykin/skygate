#include "CatalogSemanticFixtureAdapter.hpp"

#include "StringUtilities.hpp"
#include "catalog/CatalogIdentifier.hpp"
#include "catalog/CatalogSchemaRegistry.hpp"
#include "math/TimeConstants.hpp"
#include "time/AstronomicalEpoch.hpp"
#include "time/TimeScale.hpp"
#include "catalog/io/CatalogParsingUtilities.hpp"
#include "catalog/io/DelimitedCatalogParser.hpp"

#include <QString>
#include <QStringList>
#include <QStringView>

#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace skygate::ephemeris::tests {
namespace {

constexpr std::string_view kInvalidCategoryLabels[] = {
    "invalid semantic rows",
};

constexpr std::size_t kRowCountLimitFloor = 2000000;
constexpr std::size_t kMinExpectedBytesPerDataRow = 8;

[[nodiscard]] skygate::core::AstronomicalEpoch j2000CatalogEpoch() noexcept
{
    return {
        .julianDatePart1 = skygate::core::TimeConstants::kJulianDateJ2000,
        .julianDatePart2 = 0.0,
        .timeScale = skygate::core::TimeScale::Tt,
    };
}

[[nodiscard]] std::optional<double> optionalFiniteDoubleColumn(const DelimitedCatalogRow& row, const QString& name)
{
    const QString text = row.decodeColumn(name);
    if (text.trimmed().isEmpty()) {
        return std::nullopt;
    }
    return CatalogParsingUtilities::parseFiniteDouble(QStringView{text});
}

void appendNonEmptyAlias(std::vector<std::string>& aliases, const QString& text)
{
    const QString trimmed = text.trimmed();
    if (trimmed.isEmpty()) {
        return;
    }
    StringUtilities::appendUniqueIgnoreAsciiCase(aliases, CatalogParsingUtilities::toUtf8String(trimmed));
}

void appendDelimitedAliases(std::vector<std::string>& aliases, const QString& text)
{
    for (const QString& token : text.split(',', Qt::SkipEmptyParts)) {
        appendNonEmptyAlias(aliases, token);
    }
}

void appendIdentifierIfPresent(
    std::vector<CatalogIdentifier>& identifiers, const QString& namespaceName, const QString& value
)
{
    if (value.trimmed().isEmpty()) {
        return;
    }
    identifiers.push_back(
        CatalogIdentifier::make(
            CatalogParsingUtilities::toUtf8String(namespaceName), CatalogParsingUtilities::toUtf8String(value.trimmed())
        )
    );
}

[[nodiscard]] DeepSkyObjectInfo::Kind dsoKindFromColumn(const QString& text)
{
    const QString normalized = text.trimmed().toLower();
    if (normalized == "galaxy") {
        return DeepSkyObjectInfo::Kind::Galaxy;
    }
    if (normalized == "open_cluster") {
        return DeepSkyObjectInfo::Kind::OpenCluster;
    }
    if (normalized == "globular_cluster") {
        return DeepSkyObjectInfo::Kind::GlobularCluster;
    }
    if (normalized == "nebula") {
        return DeepSkyObjectInfo::Kind::Nebula;
    }
    if (normalized == "planetary_nebula") {
        return DeepSkyObjectInfo::Kind::PlanetaryNebula;
    }
    if (normalized == "asterism") {
        return DeepSkyObjectInfo::Kind::Asterism;
    }
    return DeepSkyObjectInfo::Kind::Unknown;
}

[[nodiscard]] QString invalidSample(const std::size_t rowNumber, const QString& reason)
{
    return QStringLiteral("row %1 %2").arg(QString::number(static_cast<qulonglong>(rowNumber + 1U)), reason);
}

class SemanticFixtureCatalogParser final : public ICatalogParser {
public:
    [[nodiscard]] CatalogBodyParseResult
    parse(const std::string_view data, const CatalogParseProgressCallback& progressCallback) const override
    {
        const CatalogSchemaDescriptor* schema = CatalogSchemaRegistry::find(kSchemaType);
        if (schema == nullptr) {
            CatalogBodyParseResult result;
            result.errorCode = CatalogLoadResult::ErrorCode::UnsupportedFormat;
            result.errorDetail = "Semantic test schema is not registered.";
            return result;
        }

        return DelimitedCatalogParser::run(
            data,
            DelimitedCatalogReaderOptions{
                .separator = schema->delimiter,
                .requiredColumns = schema->requiredColumns,
                .invalidErrorCode = CatalogLoadResult::ErrorCode::UnsupportedFormat,
                .rowCountLimitFloor = kRowCountLimitFloor,
                .minExpectedBytesPerDataRow = kMinExpectedBytesPerDataRow,
                .emptyInputDetail = "Semantic test payload is empty.",
                .missingColumnsDetail = "Semantic test payload is missing a required column.",
                .rowLimitDetail = "Semantic test payload exceeds the supported row limit.",
            },
            DelimitedCatalogParserOptions{
                .zeroResultCode = CatalogLoadResult::ErrorCode::NoBodies,
                .zeroResultDetail = "Semantic test payload does not contain any valid rows.",
                .formatName = schema->diagnosticName,
            },
            kInvalidCategoryLabels,
            [](const DelimitedCatalogRow& row, const std::size_t rowNumber) -> RowParseOutcome {
                const auto rightAscensionHours =
                    optionalFiniteDoubleColumn(row, QStringLiteral("right_ascension_hours"));
                const auto declinationDeg = optionalFiniteDoubleColumn(row, QStringLiteral("declination_deg"));
                const auto magnitude = optionalFiniteDoubleColumn(row, QStringLiteral("visual_mag"));
                const QString objKey = row.decodeColumn(QStringLiteral("obj_key")).trimmed();
                const QString category = row.decodeColumn(QStringLiteral("category")).trimmed().toLower();

                if (!rightAscensionHours.has_value() || !declinationDeg.has_value() || !magnitude.has_value()
                    || *rightAscensionHours < 0.0 || *rightAscensionHours >= 24.0 || *declinationDeg < -90.0
                    || *declinationDeg > 90.0) {
                    return RowParseOutcome{
                        .invalidCategoryIndex = 0,
                        .invalidSample = invalidSample(rowNumber, "has invalid coordinates or magnitude"),
                    };
                }
                if (objKey.isEmpty() || (category != "star" && category != "dso")) {
                    return RowParseOutcome{
                        .invalidCategoryIndex = 0,
                        .invalidSample = invalidSample(rowNumber, "has an invalid key or category"),
                    };
                }

                const skygate::core::EquatorialCoordinate equatorial{
                    .rightAscensionHours = *rightAscensionHours, .declinationDeg = *declinationDeg
                };
                const QString displayLabel = row.decodeColumn(QStringLiteral("display_label")).trimmed();
                const std::string sourceKey = CatalogParsingUtilities::toUtf8String(objKey);

                if (category == "star") {
                    OwnGalaxyCelestialBody body;
                    body.kind = BaseCelestialBody::Kind::Star;
                    body.visualMagnitude = *magnitude;
                    body.fixedEquatorial = equatorial;
                    body.id = "star_" + sourceKey;
                    body.identity.sourceRecordId = sourceKey;

                    appendIdentifierIfPresent(
                        body.identity.externalIdentifiers, QStringLiteral("hip"), row.decodeColumn("hip_id")
                    );
                    appendIdentifierIfPresent(
                        body.identity.externalIdentifiers, QStringLiteral("hyg"), row.decodeColumn("hyg_id")
                    );

                    CatalogStarAstrometry astrometry;
                    astrometry.referenceEquatorial = equatorial;
                    astrometry.referenceEpoch = j2000CatalogEpoch();
                    astrometry.properMotionRightAscensionMasPerYear =
                        optionalFiniteDoubleColumn(row, QStringLiteral("proper_motion_ra_mas"));
                    astrometry.properMotionDeclinationMasPerYear =
                        optionalFiniteDoubleColumn(row, QStringLiteral("proper_motion_dec_mas"));
                    astrometry.stellarParallaxMas = optionalFiniteDoubleColumn(row, QStringLiteral("parallax_mas"));
                    astrometry.radialVelocityKmPerSecond =
                        optionalFiniteDoubleColumn(row, QStringLiteral("radial_velocity_kms"));
                    if (astrometry.properMotionRightAscensionMasPerYear.has_value()
                        || astrometry.properMotionDeclinationMasPerYear.has_value()
                        || astrometry.stellarParallaxMas.has_value()
                        || astrometry.radialVelocityKmPerSecond.has_value()) {
                        body.starAstrometry = astrometry;
                    }

                    if (!displayLabel.isEmpty()) {
                        body.displayName = CatalogParsingUtilities::toUtf8String(displayLabel);
                        appendNonEmptyAlias(body.identity.aliases, displayLabel);
                    } else {
                        body.displayName = body.id;
                    }
                    appendDelimitedAliases(body.identity.aliases, row.decodeColumn(QStringLiteral("aliases")));

                    return RowParseOutcome{.body = std::move(body)};
                }

                DistantCelestialBody body;
                body.kind = BaseCelestialBody::Kind::DeepSkyObject;
                body.visualMagnitude = *magnitude;
                body.fixedEquatorial = equatorial;
                body.id = "dso_" + sourceKey;
                body.identity.sourceRecordId = sourceKey;

                appendIdentifierIfPresent(
                    body.identity.externalIdentifiers, QStringLiteral("messier"), row.decodeColumn("messier_id")
                );
                appendIdentifierIfPresent(
                    body.identity.externalIdentifiers, QStringLiteral("ngc"), row.decodeColumn("ngc_id")
                );
                appendIdentifierIfPresent(
                    body.identity.externalIdentifiers, QStringLiteral("ic"), row.decodeColumn("ic_id")
                );

                body.displayName =
                    displayLabel.isEmpty() ? body.id : CatalogParsingUtilities::toUtf8String(displayLabel);

                std::vector<std::string> aliases;
                appendDelimitedAliases(aliases, row.decodeColumn(QStringLiteral("aliases")));

                DeepSkyObjectInfo info;
                info.kind = dsoKindFromColumn(row.decodeColumn(QStringLiteral("dso_kind")));
                info.aliases = aliases;
                info.majorAxisArcmin = optionalFiniteDoubleColumn(row, QStringLiteral("major_axis_arcmin"));
                info.minorAxisArcmin = optionalFiniteDoubleColumn(row, QStringLiteral("minor_axis_arcmin"));
                info.positionAngleDeg = optionalFiniteDoubleColumn(row, QStringLiteral("position_angle_deg"));
                body.deepSkyObject = std::move(info);
                body.identity.aliases = aliases;

                return RowParseOutcome{.distantBody = std::move(body)};
            },
            progressCallback
        );
    }

    static const CatalogSourceType kSchemaType;
};

const CatalogSourceType SemanticFixtureCatalogParser::kSchemaType = static_cast<CatalogSourceType>(0xF0);

void registerSemanticSchema() noexcept
{
    static const bool registered = [] {
        CatalogSchemaRegistry::registerSchema(
            CatalogSchemaDescriptor{
                .type = SemanticFixtureCatalogParser::kSchemaType,
                .diagnosticName = "Semantic Test TSV",
                .delimiter = '\t',
                .requiredColumns =
                    {
                        QStringLiteral("obj_key"),
                        QStringLiteral("category"),
                        QStringLiteral("right_ascension_hours"),
                        QStringLiteral("declination_deg"),
                        QStringLiteral("visual_mag"),
                    },
                .createParser = []() { return std::make_unique<SemanticFixtureCatalogParser>(); },
            }
        );
        return true;
    }();
    static_cast<void>(registered);
}

}  // namespace

CatalogSourceType CatalogSemanticFixtureAdapter::schemaType() noexcept
{
    registerSemanticSchema();
    return SemanticFixtureCatalogParser::kSchemaType;
}

}  // namespace skygate::ephemeris::tests
