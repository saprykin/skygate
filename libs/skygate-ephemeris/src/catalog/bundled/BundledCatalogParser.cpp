#include "catalog/bundled/BundledCatalogParser.hpp"
#include "StringUtilities.hpp"
#include "catalog/CatalogIdentifier.hpp"
#include "catalog/bundled/BundledCatalogData.hpp"
#include "catalog/normalize/CatalogSnapshotValidator.hpp"

#include <QObject>

#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace skygate::ephemeris {
namespace {

// Zero is a sentinel in the bundled table: a zero extent means the size was
// never measured, while a zero position angle means the major axis runs
// north-south. Both screens use the snapshot validator's numeric domains so
// the parser cannot admit a value the model would reject.
[[nodiscard]] std::optional<double> extentFromTableValue(const double value) noexcept
{
    const std::optional<double> extent = value;
    if (!CatalogSnapshotValidator::isKnownPositiveOptionalValue(extent)) {
        return std::nullopt;
    }
    return extent;
}

[[nodiscard]] std::optional<double> positionAngleFromTableValue(const double value) noexcept
{
    const std::optional<double> positionAngle = value;
    if (!CatalogSnapshotValidator::isKnownNonNegativeOptionalValue(positionAngle)) {
        return std::nullopt;
    }
    return positionAngle;
}

std::vector<CatalogIdentifier> bundledMessierExternalIdentifiers(const std::string_view aliases)
{
    std::vector<CatalogIdentifier> identifiers;
    for (const std::string_view alias : StringUtilities::splitView(aliases, '|')) {
        if (const std::optional<CatalogIdentifier> designation = CatalogIdentifier::designationFromText(alias);
            designation.has_value()) {
            CatalogIdentifier::appendUnique(identifiers, *designation);
        }
    }
    return identifiers;
}

}  // namespace

CatalogBodyParseResult
BundledCatalogParser::parse(const std::string_view data, const CatalogParseProgressCallback& progressCallback) const
{
    Q_UNUSED(data);
    Q_UNUSED(progressCallback);

    const std::span<const BundledCatalogData::Entry> entries = BundledCatalogData::entries();
    const std::span<const BundledCatalogData::DeepSkyObjectEntry> messierEntries = BundledCatalogData::messierEntries();

    CatalogBodyParseResult result;
    result.bodies.reserve(entries.size() + messierEntries.size());
    result.distantBodies.reserve(messierEntries.size());
    result.orderedBodyIndexes.reserve(entries.size() + messierEntries.size());
    for (const auto& entry : entries) {
        OwnGalaxyCelestialBody body;
        body.id = std::string(entry.id);
        body.identity.sourceRecordId = std::string(entry.id);
        body.displayName = std::string(entry.displayName);
        body.kind = entry.type;
        body.visualMagnitude = entry.visualMagnitude;
        result.orderedBodyIndexes.push_back(
            CelestialBodyCatalog::OrderEntry{
                .domain = CelestialBodyCatalog::BodyDomain::OwnGalaxy,
                .bodyIndex = result.bodies.size(),
            }
        );
        result.bodies.push_back(std::move(body));
    }

    for (const auto& entry : messierEntries) {
        DistantCelestialBody body;
        body.id = std::string(entry.id);
        body.identity.sourceRecordId = std::string(entry.id);
        body.identity.externalIdentifiers = bundledMessierExternalIdentifiers(entry.aliases);
        body.displayName = std::string(entry.displayName);
        body.kind = BaseCelestialBody::Kind::DeepSkyObject;
        body.visualMagnitude = entry.visualMagnitude;
        body.fixedEquatorial = skygate::core::EquatorialCoordinate{
            .rightAscensionHours = entry.rightAscensionHours, .declinationDeg = entry.declinationDeg
        };
        const std::vector<std::string_view> aliasViews = StringUtilities::splitView(entry.aliases, '|');
        std::vector<std::string> aliases(aliasViews.begin(), aliasViews.end());
        body.identity.aliases = aliases;
        body.deepSkyObject = DeepSkyObjectInfo{
            .kind = entry.kind,
            .aliases = std::move(aliases),
            .majorAxisArcmin = extentFromTableValue(entry.majorAxisArcmin),
            .minorAxisArcmin = extentFromTableValue(entry.minorAxisArcmin),
            .positionAngleDeg = positionAngleFromTableValue(entry.positionAngleDeg),
        };
        result.orderedBodyIndexes.push_back(
            CelestialBodyCatalog::OrderEntry{
                .domain = CelestialBodyCatalog::BodyDomain::Distant,
                .bodyIndex = result.distantBodies.size(),
            }
        );
        result.distantBodies.push_back(std::move(body));
    }

    const std::size_t bodyCount = result.bodies.size() + result.distantBodies.size();
    result.diagnostics.processedRowCount = bodyCount;
    result.diagnostics.parsedBodyCount = bodyCount;
    return result;
}

}  // namespace skygate::ephemeris
