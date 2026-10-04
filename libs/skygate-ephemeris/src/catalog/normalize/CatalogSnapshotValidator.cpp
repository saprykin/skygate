#include "CatalogSnapshotValidator.hpp"

#include "CatalogBodyNormalization.hpp"
#include "DistantCelestialBody.hpp"
#include "EquatorialCoordinate.hpp"
#include "OwnGalaxyCelestialBody.hpp"
#include "StringUtilities.hpp"
#include "catalog/CatalogIdentifier.hpp"
#include "catalog/CatalogStarAstrometry.hpp"
#include "time/AstronomicalEpoch.hpp"

#include <cmath>
#include <cstddef>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace skygate::ephemeris {
namespace {

constexpr double kRightAscensionMinimumHours = 0.0;
constexpr double kRightAscensionMaximumHours = 24.0;
constexpr double kDeclinationMinimumDeg = -90.0;
constexpr double kDeclinationMaximumDeg = 90.0;

[[nodiscard]] bool isValidEquatorial(const skygate::core::EquatorialCoordinate& coordinate) noexcept
{
    return std::isfinite(coordinate.rightAscensionHours) && std::isfinite(coordinate.declinationDeg)
           && coordinate.rightAscensionHours >= kRightAscensionMinimumHours
           && coordinate.rightAscensionHours < kRightAscensionMaximumHours
           && coordinate.declinationDeg >= kDeclinationMinimumDeg
           && coordinate.declinationDeg <= kDeclinationMaximumDeg;
}

[[nodiscard]] bool isValidEpoch(const skygate::core::AstronomicalEpoch& epoch) noexcept
{
    return std::isfinite(epoch.julianDatePart1) && std::isfinite(epoch.julianDatePart2)
           && CatalogSnapshotValidator::isKnownTimeScale(epoch.timeScale);
}

[[nodiscard]] bool isValidOptionalFiniteDouble(const std::optional<double> value) noexcept
{
    return !value.has_value() || std::isfinite(*value);
}

[[nodiscard]] bool isValidOptionalPositiveDouble(const std::optional<double> value) noexcept
{
    return !value.has_value() || (std::isfinite(*value) && *value > 0.0);
}

void normalizeAliases(std::vector<std::string>& aliases)
{
    std::vector<std::string> normalized;
    normalized.reserve(aliases.size());
    for (const std::string& alias : aliases) {
        const std::string_view trimmed = StringUtilities::trimAsciiWhitespace(alias);
        if (!trimmed.empty()) {
            normalized.emplace_back(trimmed);
        }
    }
    aliases = std::move(normalized);
}

void normalizeIdentity(CatalogObjectIdentity& identity)
{
    std::vector<CatalogIdentifier> identifiers;
    identifiers.reserve(identity.externalIdentifiers.size());
    for (const CatalogIdentifier& identifier : identity.externalIdentifiers) {
        CatalogIdentifier normalized = CatalogIdentifier::make(identifier.namespaceName, identifier.value);
        if (!normalized.empty()) {
            identifiers.push_back(std::move(normalized));
        }
    }
    identity.externalIdentifiers = std::move(identifiers);
    normalizeAliases(identity.aliases);
}

void normalizeOwnGalaxyBody(OwnGalaxyCelestialBody& body)
{
    CatalogBodyNormalization::apply(body);
    normalizeIdentity(body.identity);
}

void normalizeDistantBody(DistantCelestialBody& body)
{
    normalizeIdentity(body.identity);
}

[[nodiscard]] bool validateBody(const BaseCelestialBody& body, std::string& errorDetail)
{
    if (!CatalogSnapshotValidator::isKnownBodyKind(body.kind)) {
        errorDetail = "catalog body has an unknown kind value.";
        return false;
    }
    if (StringUtilities::trimAsciiWhitespace(body.id).empty()) {
        errorDetail = "catalog body has an empty canonical id.";
        return false;
    }
    if (std::isinf(body.visualMagnitude)) {
        errorDetail = "catalog body '" + body.id + "' has an infinite visual magnitude.";
        return false;
    }
    for (const CatalogIdentifier& identifier : body.identity.externalIdentifiers) {
        if (identifier.empty()) {
            errorDetail = "catalog body '" + body.id + "' has an incomplete external identifier.";
            return false;
        }
    }

    if (const skygate::core::EquatorialCoordinate* fixed = body.fixedEquatorialCoordinate();
        fixed != nullptr && !isValidEquatorial(*fixed)) {
        errorDetail = "catalog body '" + body.id + "' has an invalid fixed equatorial coordinate.";
        return false;
    }

    if (const CatalogStarAstrometry* astrometry = body.catalogStarAstrometry(); astrometry != nullptr) {
        if (!isValidEquatorial(astrometry->referenceEquatorial)) {
            errorDetail = "catalog body '" + body.id + "' has an invalid astrometry reference coordinate.";
            return false;
        }
        if (!isValidEpoch(astrometry->referenceEpoch)) {
            errorDetail = "catalog body '" + body.id + "' has an invalid astrometry reference epoch.";
            return false;
        }
        if (!isValidOptionalFiniteDouble(astrometry->properMotionRightAscensionMasPerYear)
            || !isValidOptionalFiniteDouble(astrometry->properMotionDeclinationMasPerYear)
            || !isValidOptionalFiniteDouble(astrometry->radialVelocityKmPerSecond)) {
            errorDetail = "catalog body '" + body.id + "' has a non-finite astrometry value.";
            return false;
        }
        if (!isValidOptionalPositiveDouble(astrometry->stellarParallaxMas)) {
            errorDetail = "catalog body '" + body.id + "' has a non-positive stellar parallax.";
            return false;
        }
        if (astrometry->validityRange.has_value()
            && (!isValidEpoch(astrometry->validityRange->start) || !isValidEpoch(astrometry->validityRange->end))) {
            errorDetail = "catalog body '" + body.id + "' has an invalid astrometry validity range.";
            return false;
        }
    }

    if (const DeepSkyObjectInfo* info = body.deepSkyObjectInfo(); info != nullptr) {
        if (!CatalogSnapshotValidator::isKnownDeepSkyObjectKind(info->kind)) {
            errorDetail = "catalog body '" + body.id + "' has an unknown deep-sky object kind.";
            return false;
        }
        if (!isValidOptionalPositiveDouble(info->majorAxisArcmin)
            || !isValidOptionalPositiveDouble(info->minorAxisArcmin)) {
            errorDetail = "catalog body '" + body.id + "' has a non-positive deep-sky object extent.";
            return false;
        }
        if (!isValidOptionalFiniteDouble(info->positionAngleDeg)) {
            errorDetail = "catalog body '" + body.id + "' has a non-finite deep-sky object position angle.";
            return false;
        }
    }

    return true;
}

[[nodiscard]] bool validateOrderEntries(
    const std::span<const CelestialBodyCatalog::OrderEntry> orderedBodyIndexes,
    const std::size_t ownGalaxyBodyCount,
    const std::size_t distantBodyCount,
    std::string& errorDetail
)
{
    for (std::size_t index = 0; index < orderedBodyIndexes.size(); ++index) {
        const CelestialBodyCatalog::OrderEntry& entry = orderedBodyIndexes[index];
        if (!CatalogSnapshotValidator::isOrderEntryValid(
                entry.domain, entry.bodyIndex, ownGalaxyBodyCount, distantBodyCount
            )) {
            errorDetail =
                "order entry " + std::to_string(index) + " references an unknown domain or out-of-range body.";
            return false;
        }
    }
    return true;
}

}  // namespace

bool CatalogSnapshotValidator::isKnownBodyKind(const BaseCelestialBody::Kind kind) noexcept
{
    switch (kind) {
    case BaseCelestialBody::Kind::Star:
    case BaseCelestialBody::Kind::Planet:
    case BaseCelestialBody::Kind::Moon:
    case BaseCelestialBody::Kind::Sun:
    case BaseCelestialBody::Kind::Constellation:
    case BaseCelestialBody::Kind::DeepSkyObject:
        return true;
    }
    return false;
}

bool CatalogSnapshotValidator::isKnownDeepSkyObjectKind(const DeepSkyObjectInfo::Kind kind) noexcept
{
    switch (kind) {
    case DeepSkyObjectInfo::Kind::Unknown:
    case DeepSkyObjectInfo::Kind::Galaxy:
    case DeepSkyObjectInfo::Kind::OpenCluster:
    case DeepSkyObjectInfo::Kind::GlobularCluster:
    case DeepSkyObjectInfo::Kind::Nebula:
    case DeepSkyObjectInfo::Kind::PlanetaryNebula:
    case DeepSkyObjectInfo::Kind::Asterism:
        return true;
    }
    return false;
}

bool CatalogSnapshotValidator::isKnownIdScope(const CatalogObjectIdentity::IdScope idScope) noexcept
{
    switch (idScope) {
    case CatalogObjectIdentity::IdScope::Global:
    case CatalogObjectIdentity::IdScope::SourceLocal:
        return true;
    }
    return false;
}

bool CatalogSnapshotValidator::isKnownTimeScale(const skygate::core::TimeScale timeScale) noexcept
{
    switch (timeScale) {
    case skygate::core::TimeScale::Utc:
    case skygate::core::TimeScale::Tai:
    case skygate::core::TimeScale::Tt:
    case skygate::core::TimeScale::Tdb:
    case skygate::core::TimeScale::Ut1:
        return true;
    }
    return false;
}

bool CatalogSnapshotValidator::isOrderEntryValid(
    const CelestialBodyCatalog::BodyDomain domain,
    const std::size_t bodyIndex,
    const std::size_t ownGalaxyBodyCount,
    const std::size_t distantBodyCount
) noexcept
{
    switch (domain) {
    case CelestialBodyCatalog::BodyDomain::OwnGalaxy:
        return bodyIndex < ownGalaxyBodyCount;
    case CelestialBodyCatalog::BodyDomain::Distant:
        return bodyIndex < distantBodyCount;
    }
    return false;
}

CatalogSnapshotValidator::Report CatalogSnapshotValidator::validate(std::vector<OwnGalaxyCelestialBody>& bodies)
{
    Report report;
    for (OwnGalaxyCelestialBody& body : bodies) {
        normalizeOwnGalaxyBody(body);
    }
    for (const OwnGalaxyCelestialBody& body : bodies) {
        if (!validateBody(body, report.errorDetail)) {
            report.ok = false;
            return report;
        }
    }
    return report;
}

CatalogSnapshotValidator::Report CatalogSnapshotValidator::validate(
    std::vector<OwnGalaxyCelestialBody>& ownGalaxyBodies,
    std::vector<DistantCelestialBody>& distantBodies,
    const std::vector<CelestialBodyCatalog::OrderEntry>& orderedBodyIndexes
)
{
    Report report;
    for (OwnGalaxyCelestialBody& body : ownGalaxyBodies) {
        normalizeOwnGalaxyBody(body);
    }
    for (DistantCelestialBody& body : distantBodies) {
        normalizeDistantBody(body);
    }
    for (const OwnGalaxyCelestialBody& body : ownGalaxyBodies) {
        if (!validateBody(body, report.errorDetail)) {
            report.ok = false;
            return report;
        }
    }
    for (const DistantCelestialBody& body : distantBodies) {
        if (!validateBody(body, report.errorDetail)) {
            report.ok = false;
            return report;
        }
    }
    if (!validateOrderEntries(orderedBodyIndexes, ownGalaxyBodies.size(), distantBodies.size(), report.errorDetail)) {
        report.ok = false;
        return report;
    }
    return report;
}

}  // namespace skygate::ephemeris
