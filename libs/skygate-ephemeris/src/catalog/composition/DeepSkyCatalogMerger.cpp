#include "DeepSkyCatalogMerger.hpp"
#include "StringUtilities.hpp"
#include "catalog/CatalogIdentifier.hpp"
#include "catalog/CatalogIdentityIndex.hpp"
#include "catalog/CatalogObjectIdentity.hpp"

#include <QLoggingCategory>
#include <QString>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace skygate::ephemeris {
namespace {

Q_LOGGING_CATEGORY(skygateCatalogCompositionLog, "skygate.catalog.composition")

constexpr double kCoordinateTolerance = 1e-9;

// Accumulates deduplicated bodies of one source while keeping logical
// positions stable so an identity index can address each survivor by value.
struct MergeAccumulator final {
    std::vector<OwnGalaxyCelestialBody> ownGalaxyBodies;
    std::vector<DistantCelestialBody> distantBodies;
    std::vector<CatalogCompositionSource> sourceKinds;
    std::vector<bool> isDistant;
    std::vector<std::size_t> domainIndex;

    [[nodiscard]] std::size_t append(const BaseCelestialBody& body, const CatalogCompositionSource sourceKind)
    {
        if (body.kind == BaseCelestialBody::Kind::DeepSkyObject) {
            distantBodies.push_back(CelestialBodyCatalog::copyDistantBody(body));
            domainIndex.push_back(distantBodies.size() - 1U);
            isDistant.push_back(true);
        } else {
            ownGalaxyBodies.push_back(CelestialBodyCatalog::copyOwnGalaxyBody(body));
            domainIndex.push_back(ownGalaxyBodies.size() - 1U);
            isDistant.push_back(false);
        }
        sourceKinds.push_back(sourceKind);
        return sourceKinds.size() - 1U;
    }

    [[nodiscard]] BaseCelestialBody& at(const std::size_t position)
    {
        if (isDistant[position]) {
            return distantBodies[domainIndex[position]];
        }
        return ownGalaxyBodies[domainIndex[position]];
    }

    [[nodiscard]] const BaseCelestialBody& at(const std::size_t position) const
    {
        if (isDistant[position]) {
            return distantBodies[domainIndex[position]];
        }
        return ownGalaxyBodies[domainIndex[position]];
    }
};

struct MatchDecision final {
    enum class Action {
        Append,
        Merge
    };

    Action action = Action::Append;
    std::size_t matchIndex = 0;
};

QString bodyLabel(const BaseCelestialBody& body)
{
    return QString::fromStdString(body.id);
}

void appendIdentifierUnique(std::vector<CatalogIdentifier>& identifiers, const CatalogIdentifier& identifier)
{
    if (identifier.empty()) {
        return;
    }

    const bool present =
        std::any_of(identifiers.begin(), identifiers.end(), [&identifier](const CatalogIdentifier& existing) {
            return existing == identifier;
        });
    if (!present) {
        identifiers.push_back(identifier);
    }
}

// Retains the union of identifiers from both records, including several
// values in the same namespace (a single object can carry multiple NGC or IC
// designations).
void mergeExternalIdentifiersInto(CatalogObjectIdentity& winner, const CatalogObjectIdentity& loser)
{
    for (const CatalogIdentifier& identifier : loser.externalIdentifiers) {
        appendIdentifierUnique(winner.externalIdentifiers, identifier);
    }
}

void mergeAliasesInto(std::vector<std::string>& winner, const std::vector<std::string>& loser)
{
    for (const std::string& alias : loser) {
        StringUtilities::appendUniqueIgnoreAsciiCase(winner, alias);
    }
}

void mergeIdentityInto(CatalogObjectIdentity& winner, const CatalogObjectIdentity& loser)
{
    mergeExternalIdentifiersInto(winner, loser);
    mergeAliasesInto(winner.aliases, loser.aliases);
}

// True when two records disagree about an authoritative namespace: both claim
// at least one value for the namespace but their value sets do not overlap.
// An overlapping value would already have been resolved authoritatively, so
// this only distinguishes otherwise non-authoritative alias matches.
bool hasContradictoryIdentifiers(const CatalogObjectIdentity& lhs, const CatalogObjectIdentity& rhs)
{
    std::vector<std::string_view> checkedNamespaces;
    for (const CatalogIdentifier& lhsIdentifier : lhs.externalIdentifiers) {
        if (lhsIdentifier.empty()) {
            continue;
        }
        if (std::find(checkedNamespaces.begin(), checkedNamespaces.end(), lhsIdentifier.namespaceName)
            != checkedNamespaces.end()) {
            continue;
        }
        checkedNamespaces.push_back(lhsIdentifier.namespaceName);

        std::vector<std::string_view> lhsValues;
        std::vector<std::string_view> rhsValues;
        for (const CatalogIdentifier& identifier : lhs.externalIdentifiers) {
            if (!identifier.empty() && identifier.namespaceName == lhsIdentifier.namespaceName) {
                lhsValues.push_back(identifier.value);
            }
        }
        for (const CatalogIdentifier& identifier : rhs.externalIdentifiers) {
            if (!identifier.empty() && identifier.namespaceName == lhsIdentifier.namespaceName) {
                rhsValues.push_back(identifier.value);
            }
        }
        if (rhsValues.empty()) {
            continue;
        }

        const bool sharesValue =
            std::any_of(lhsValues.begin(), lhsValues.end(), [&rhsValues](const std::string_view lhsValue) {
                return std::find(rhsValues.begin(), rhsValues.end(), lhsValue) != rhsValues.end();
            });
        if (!sharesValue) {
            return true;
        }
    }
    return false;
}

bool coordinatesConflict(const skygate::core::EquatorialCoordinate& lhs, const skygate::core::EquatorialCoordinate& rhs)
{
    return std::abs(lhs.rightAscensionHours - rhs.rightAscensionHours) > kCoordinateTolerance
           || std::abs(lhs.declinationDeg - rhs.declinationDeg) > kCoordinateTolerance;
}

void mergeOwnGalaxyInPlace(OwnGalaxyCelestialBody& winner, const BaseCelestialBody& loser, bool& conflict)
{
    mergeIdentityInto(winner.identity, loser.identity);
    if (winner.displayName.empty() && !loser.displayName.empty()) {
        winner.displayName = loser.displayName;
    }

    const skygate::core::EquatorialCoordinate* loserFixed = loser.fixedEquatorialCoordinate();
    if (!winner.fixedEquatorial.has_value() && loserFixed != nullptr) {
        winner.fixedEquatorial = *loserFixed;
    } else if (
        winner.fixedEquatorial.has_value() && loserFixed != nullptr
        && coordinatesConflict(*winner.fixedEquatorial, *loserFixed)
    ) {
        conflict = true;
    }

    const CatalogStarAstrometry* loserAstrometry = loser.catalogStarAstrometry();
    if (!winner.starAstrometry.has_value() && loserAstrometry != nullptr) {
        winner.starAstrometry = *loserAstrometry;
    } else if (
        winner.starAstrometry.has_value() && loserAstrometry != nullptr
        && coordinatesConflict(winner.starAstrometry->referenceEquatorial, loserAstrometry->referenceEquatorial)
    ) {
        conflict = true;
    }
}

void mergeDistantInPlace(DistantCelestialBody& winner, const BaseCelestialBody& loser, bool& conflict)
{
    mergeIdentityInto(winner.identity, loser.identity);
    if (winner.displayName.empty() && !loser.displayName.empty()) {
        winner.displayName = loser.displayName;
    }

    const skygate::core::EquatorialCoordinate* loserFixed = loser.fixedEquatorialCoordinate();
    if (!winner.fixedEquatorial.has_value() && loserFixed != nullptr) {
        winner.fixedEquatorial = *loserFixed;
    } else if (
        winner.fixedEquatorial.has_value() && loserFixed != nullptr
        && coordinatesConflict(*winner.fixedEquatorial, *loserFixed)
    ) {
        conflict = true;
    }

    const DeepSkyObjectInfo* loserDeepSky = loser.deepSkyObjectInfo();
    if (!winner.deepSkyObject.has_value() && loserDeepSky != nullptr) {
        winner.deepSkyObject = *loserDeepSky;
    } else if (winner.deepSkyObject.has_value() && loserDeepSky != nullptr) {
        DeepSkyObjectInfo& merged = *winner.deepSkyObject;
        if (merged.kind == DeepSkyObjectInfo::Kind::Unknown && loserDeepSky->kind != DeepSkyObjectInfo::Kind::Unknown) {
            merged.kind = loserDeepSky->kind;
        }
        if (!merged.majorAxisArcmin.has_value() && loserDeepSky->majorAxisArcmin.has_value()) {
            merged.majorAxisArcmin = loserDeepSky->majorAxisArcmin;
        }
        if (!merged.minorAxisArcmin.has_value() && loserDeepSky->minorAxisArcmin.has_value()) {
            merged.minorAxisArcmin = loserDeepSky->minorAxisArcmin;
        }
        if (!merged.positionAngleDeg.has_value() && loserDeepSky->positionAngleDeg.has_value()) {
            merged.positionAngleDeg = loserDeepSky->positionAngleDeg;
        }
        mergeAliasesInto(merged.aliases, loserDeepSky->aliases);
    }
}

void mergeSurvivorInPlace(BaseCelestialBody& winner, const BaseCelestialBody& loser, bool& conflict)
{
    if (winner.kind == BaseCelestialBody::Kind::DeepSkyObject) {
        mergeDistantInPlace(static_cast<DistantCelestialBody&>(winner), loser, conflict);
        return;
    }
    mergeOwnGalaxyInPlace(static_cast<OwnGalaxyCelestialBody&>(winner), loser, conflict);
}

void logIncompatibleKind(const BaseCelestialBody& existing, const BaseCelestialBody& incoming)
{
    qCWarning(skygateCatalogCompositionLog).noquote()
        << "Catalog composition kept" << bodyLabel(incoming) << "distinct from" << bodyLabel(existing)
        << "because the shared identity is used by incompatible object kinds.";
}

void logAmbiguousAlias(const BaseCelestialBody& existing, const BaseCelestialBody& incoming)
{
    qCWarning(skygateCatalogCompositionLog).noquote()
        << "Catalog composition kept" << bodyLabel(incoming) << "distinct from" << bodyLabel(existing)
        << "because their shared alias is ambiguous across authoritative identifiers.";
}

void logAmbiguousResolution(const CatalogIdentityIndex::Resolution& resolution, const BaseCelestialBody& incoming)
{
    qCWarning(skygateCatalogCompositionLog).noquote()
        << "Catalog composition kept" << bodyLabel(incoming) << "distinct because its identity matched"
        << resolution.candidates.size() << "different bodies.";
}

void logConflictingAstrometry(const BaseCelestialBody& winner, const BaseCelestialBody& loser)
{
    qCWarning(skygateCatalogCompositionLog).noquote()
        << "Catalog composition kept the coordinates of" << bodyLabel(winner) << "over conflicting astrometry from"
        << bodyLabel(loser) << ".";
}

// Decides how `incoming` resolves against the bodies already indexed, logging
// any keep-distinct decision.
MatchDecision
evaluateMatch(const MergeAccumulator& accumulator, const CatalogIdentityIndex& index, const BaseCelestialBody& incoming)
{
    MatchDecision decision;
    const CatalogIdentityIndex::Resolution resolution = index.resolve(incoming);
    if (resolution.hasSingleMatch()) {
        const BaseCelestialBody& existing = accumulator.at(resolution.index);
        decision.matchIndex = resolution.index;
        if (incoming.kind != existing.kind) {
            logIncompatibleKind(existing, incoming);
            return decision;
        }

        if (resolution.kind == CatalogIdentityIndex::Resolution::MatchKind::Alias
            && hasContradictoryIdentifiers(existing.identity, incoming.identity)) {
            logAmbiguousAlias(existing, incoming);
            return decision;
        }

        decision.action = MatchDecision::Action::Merge;
        return decision;
    }

    if (resolution.isAmbiguous()) {
        logAmbiguousResolution(resolution, incoming);
    }
    return decision;
}

std::size_t appendDeduped(
    MergeAccumulator& accumulator,
    CatalogIdentityIndex& index,
    const BaseCelestialBody& body,
    const CatalogCompositionSource sourceKind
)
{
    const MatchDecision decision = evaluateMatch(accumulator, index, body);
    if (decision.action == MatchDecision::Action::Merge) {
        bool conflict = false;
        mergeSurvivorInPlace(accumulator.at(decision.matchIndex), body, conflict);
        if (conflict) {
            logConflictingAstrometry(accumulator.at(decision.matchIndex), body);
        }
        index.add(body, decision.matchIndex);
        return decision.matchIndex;
    }

    const std::size_t position = accumulator.append(body, sourceKind);
    index.add(body, position);
    return position;
}

void pushOwnGalaxyBody(
    DeepSkyCatalogMergeResult& result, const OwnGalaxyCelestialBody& body, const CatalogCompositionSource sourceKind
)
{
    result.orderedBodyIndexes.push_back(
        CelestialBodyCatalog::OrderEntry{
            .domain = CelestialBodyCatalog::BodyDomain::OwnGalaxy, .bodyIndex = result.ownGalaxyBodies.size()
        }
    );
    result.ownGalaxyBodies.push_back(body);
    result.sourceKinds.push_back(sourceKind);
}

void pushDistantBody(
    DeepSkyCatalogMergeResult& result, const DistantCelestialBody& body, const CatalogCompositionSource sourceKind
)
{
    result.orderedBodyIndexes.push_back(
        CelestialBodyCatalog::OrderEntry{
            .domain = CelestialBodyCatalog::BodyDomain::Distant, .bodyIndex = result.distantBodies.size()
        }
    );
    result.distantBodies.push_back(body);
    result.sourceKinds.push_back(sourceKind);
}

}  // namespace

DeepSkyCatalogMergeResult DeepSkyCatalogMerger::merge(
    const std::span<const BaseCelestialBody* const> activeBodies,
    const std::span<const CatalogCompositionSource> activeSourceKinds,
    const std::span<const BaseCelestialBody* const> deepSkyBodies
)
{
    MergeAccumulator active;
    CatalogIdentityIndex activeIndex;
    for (std::size_t index = 0; index < activeBodies.size(); ++index) {
        const BaseCelestialBody* body = activeBodies[index];
        if (body == nullptr) {
            continue;
        }
        const CatalogCompositionSource sourceKind =
            index < activeSourceKinds.size() ? activeSourceKinds[index] : CatalogCompositionSource::Primary;
        appendDeduped(active, activeIndex, *body, sourceKind);
    }

    MergeAccumulator deepSky;
    CatalogIdentityIndex deepSkyIndex;
    for (const BaseCelestialBody* body : deepSkyBodies) {
        if (body == nullptr || body->kind != BaseCelestialBody::Kind::DeepSkyObject) {
            continue;
        }
        appendDeduped(deepSky, deepSkyIndex, *body, CatalogCompositionSource::DeepSky);
    }

    std::vector<bool> activeDsoConsumed(active.sourceKinds.size(), false);
    std::vector<DistantCelestialBody> mergedDeepSkyBodies;
    mergedDeepSkyBodies.reserve(deepSky.distantBodies.size());
    for (const DistantCelestialBody& deepSkyBody : deepSky.distantBodies) {
        DistantCelestialBody output = CelestialBodyCatalog::copyDistantBody(deepSkyBody);

        const MatchDecision decision = evaluateMatch(active, activeIndex, deepSkyBody);
        if (decision.action == MatchDecision::Action::Merge) {
            activeDsoConsumed[decision.matchIndex] = true;
            bool conflict = false;
            mergeDistantInPlace(output, active.at(decision.matchIndex), conflict);
            if (conflict) {
                logConflictingAstrometry(output, active.at(decision.matchIndex));
            }
        }

        mergedDeepSkyBodies.push_back(std::move(output));
    }

    DeepSkyCatalogMergeResult result;
    result.ownGalaxyBodies.reserve(active.ownGalaxyBodies.size());
    result.distantBodies.reserve(active.distantBodies.size() + mergedDeepSkyBodies.size());
    result.orderedBodyIndexes.reserve(active.sourceKinds.size() + mergedDeepSkyBodies.size());
    result.sourceKinds.reserve(active.sourceKinds.size() + mergedDeepSkyBodies.size());

    for (std::size_t position = 0; position < active.sourceKinds.size(); ++position) {
        if (active.isDistant[position]) {
            continue;
        }
        pushOwnGalaxyBody(result, active.ownGalaxyBodies[active.domainIndex[position]], active.sourceKinds[position]);
    }
    for (std::size_t position = 0; position < active.sourceKinds.size(); ++position) {
        if (!active.isDistant[position] || activeDsoConsumed[position]) {
            continue;
        }
        pushDistantBody(result, active.distantBodies[active.domainIndex[position]], active.sourceKinds[position]);
    }
    for (const DistantCelestialBody& body : mergedDeepSkyBodies) {
        pushDistantBody(result, body, CatalogCompositionSource::DeepSky);
    }

    return result;
}

}  // namespace skygate::ephemeris
