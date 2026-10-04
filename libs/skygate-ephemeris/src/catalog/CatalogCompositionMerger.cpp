#include "CatalogCompositionMerger.hpp"

#include "StringUtilities.hpp"
#include "catalog/CatalogIdentifier.hpp"
#include "catalog/CatalogIdentityIndex.hpp"
#include "catalog/CatalogObjectIdentity.hpp"
#include "catalog/composition/CoreBodyCatalogAugmenter.hpp"

#include <QLoggingCategory>
#include <QString>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace skygate::ephemeris {
namespace {

Q_LOGGING_CATEGORY(skygateCatalogCompositionLog, "skygate.catalog.composition")

constexpr double kCoordinateTolerance = 1e-9;

// A source-local id is a record key of exactly one source instance: the
// generator restarts in every payload (for example hyg_auto_1 for every
// anonymous HYG parse), so the bare id must not act as a cross-source object
// key. Appending the owning source instance id yields a key that is stable
// across reloads of that instance (the instance id is durable) and distinct
// from every other instance. Generated ids never contain '@', so the first
// '@' always separates the generated id from the instance id and equal
// generated ids from different instances can never qualify to the same key.
[[nodiscard]] std::string qualifiedSourceLocalId(const std::string_view generatedId, const std::string_view sourceId)
{
    std::string qualified;
    qualified.reserve(generatedId.size() + sourceId.size() + 1U);
    qualified.append(generatedId);
    qualified.push_back('@');
    qualified.append(sourceId);
    return qualified;
}

// Owns the instance-qualified copy of a body whose canonical id is source
// local. Bodies with global identities are referenced without copying, so the
// qualification cost applies only to the generated-id rows that need it.
class SourceScopedBody final {
public:
    SourceScopedBody(const BaseCelestialBody& body, const std::string_view sourceId)
    {
        if (body.identity.idScope != CatalogObjectIdentity::IdScope::SourceLocal) {
            m_body = &body;
            return;
        }

        if (body.kind == BaseCelestialBody::Kind::DeepSkyObject) {
            m_distant = CelestialBodyCatalog::copyDistantBody(body);
            m_distant->id = qualifiedSourceLocalId(m_distant->id, sourceId);
            m_distant->identity.idScope = CatalogObjectIdentity::IdScope::Global;
            m_body = &*m_distant;
            return;
        }

        m_ownGalaxy = CelestialBodyCatalog::copyOwnGalaxyBody(body);
        m_ownGalaxy->id = qualifiedSourceLocalId(m_ownGalaxy->id, sourceId);
        m_ownGalaxy->identity.idScope = CatalogObjectIdentity::IdScope::Global;
        m_body = &*m_ownGalaxy;
    }

    [[nodiscard]] const BaseCelestialBody& body() const noexcept
    {
        return *m_body;
    }

private:
    std::optional<OwnGalaxyCelestialBody> m_ownGalaxy;
    std::optional<DistantCelestialBody> m_distant;
    const BaseCelestialBody* m_body = nullptr;
};

// Accumulates deduplicated bodies of one or more sources while keeping logical
// positions stable so an identity index can address each survivor by value.
// A position can be vacated when a later replacing source wins; vacated
// positions are skipped during final assembly.
struct MergeAccumulator final {
    std::vector<OwnGalaxyCelestialBody> ownGalaxyBodies;
    std::vector<DistantCelestialBody> distantBodies;
    std::vector<std::string> sourceIds;
    std::vector<std::vector<std::string>> contributorSourceIds;
    std::vector<bool> isDistant;
    std::vector<std::size_t> domainIndex;
    std::vector<bool> active;

    [[nodiscard]] std::size_t
    append(const BaseCelestialBody& body, std::string sourceId, std::vector<std::string> priorContributors = {})
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
        sourceIds.push_back(std::move(sourceId));

        std::vector<std::string> contributors;
        contributors.reserve(priorContributors.size() + 1U);
        contributors.push_back(sourceIds.back());
        for (std::string& prior : priorContributors) {
            if (std::find(contributors.begin(), contributors.end(), prior) == contributors.end()) {
                contributors.push_back(std::move(prior));
            }
        }
        contributorSourceIds.push_back(std::move(contributors));
        active.push_back(true);
        return sourceIds.size() - 1U;
    }

    void vacate(const std::size_t position)
    {
        active[position] = false;
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

// Appends `body` into `accumulator`, merging into the first matching survivor
// (earlier-wins) within a single source. Source-local generated ids are
// qualified with `sourceId` before resolution so equal counters from other
// sources never match.
std::size_t appendDeduped(
    MergeAccumulator& accumulator,
    CatalogIdentityIndex& index,
    const BaseCelestialBody& incoming,
    const std::string_view sourceId
)
{
    const SourceScopedBody scoped{incoming, sourceId};
    const BaseCelestialBody& body = scoped.body();
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

    const std::size_t position = accumulator.append(body, std::string(sourceId));
    index.add(body, position);
    return position;
}

// Builds an identity index over the currently active (non-vacated) positions.
CatalogIdentityIndex buildActiveIndex(const MergeAccumulator& accumulator)
{
    CatalogIdentityIndex index;
    for (std::size_t position = 0; position < accumulator.active.size(); ++position) {
        if (accumulator.active[position]) {
            index.add(accumulator.at(position), position);
        }
    }
    return index;
}

bool participates(const CatalogCompositionPolicy policy, const BaseCelestialBody& body)
{
    switch (policy) {
    case CatalogCompositionPolicy::DeepSkyOnly:
        return body.kind == BaseCelestialBody::Kind::DeepSkyObject;
    case CatalogCompositionPolicy::AugmentCore:
        return body.kind != BaseCelestialBody::Kind::DeepSkyObject;
    case CatalogCompositionPolicy::Merge:
        return true;
    }
    return true;
}

bool hasAnyMatch(const CatalogIdentityIndex& index, const BaseCelestialBody& body)
{
    const CatalogIdentityIndex::Resolution resolution = index.resolve(body);
    return resolution.hasSingleMatch() || resolution.isAmbiguous();
}

void pushOwnGalaxyBody(
    CatalogCompositionMergeResult& result,
    const OwnGalaxyCelestialBody& body,
    const std::string_view sourceId,
    const std::vector<std::string>& contributorSourceIds
)
{
    result.orderedBodyIndexes.push_back(
        CelestialBodyCatalog::OrderEntry{
            .domain = CelestialBodyCatalog::BodyDomain::OwnGalaxy, .bodyIndex = result.ownGalaxyBodies.size()
        }
    );
    result.ownGalaxyBodies.push_back(body);
    result.sourceIds.emplace_back(sourceId);
    result.contributorSourceIds.push_back(contributorSourceIds);
}

void pushDistantBody(
    CatalogCompositionMergeResult& result,
    const DistantCelestialBody& body,
    const std::string_view sourceId,
    const std::vector<std::string>& contributorSourceIds
)
{
    result.orderedBodyIndexes.push_back(
        CelestialBodyCatalog::OrderEntry{
            .domain = CelestialBodyCatalog::BodyDomain::Distant, .bodyIndex = result.distantBodies.size()
        }
    );
    result.distantBodies.push_back(body);
    result.sourceIds.emplace_back(sourceId);
    result.contributorSourceIds.push_back(contributorSourceIds);
}

void assembleResult(CatalogCompositionMergeResult& result, const MergeAccumulator& accumulator)
{
    result.ownGalaxyBodies.reserve(accumulator.ownGalaxyBodies.size());
    result.distantBodies.reserve(accumulator.distantBodies.size());
    result.orderedBodyIndexes.reserve(accumulator.sourceIds.size());
    result.sourceIds.reserve(accumulator.sourceIds.size());
    result.contributorSourceIds.reserve(accumulator.sourceIds.size());

    for (std::size_t position = 0; position < accumulator.sourceIds.size(); ++position) {
        if (!accumulator.active[position]) {
            continue;
        }
        if (accumulator.isDistant[position]) {
            pushDistantBody(
                result,
                accumulator.distantBodies[accumulator.domainIndex[position]],
                accumulator.sourceIds[position],
                accumulator.contributorSourceIds[position]
            );
        } else {
            pushOwnGalaxyBody(
                result,
                accumulator.ownGalaxyBodies[accumulator.domainIndex[position]],
                accumulator.sourceIds[position],
                accumulator.contributorSourceIds[position]
            );
        }
    }
}

}  // namespace

CatalogCompositionMergeResult CatalogCompositionMerger::mergeCollection(const CatalogCompositionRequest& request)
{
    MergeAccumulator accumulator;
    bool augmentCoreEnabled = false;
    std::string augmentCoreSourceId;
    bool hasStar = false;

    for (const CatalogCompositionSourceEntry& source : request.sources) {
        if (!source.enabled || source.catalog == nullptr) {
            continue;
        }

        if (source.policy == CatalogCompositionPolicy::AugmentCore) {
            augmentCoreEnabled = true;
            if (augmentCoreSourceId.empty()) {
                augmentCoreSourceId = source.sourceId;
            }

            CatalogIdentityIndex index = buildActiveIndex(accumulator);
            for (const BaseCelestialBody* body : source.catalog->bodies()) {
                if (body == nullptr || !participates(source.policy, *body)) {
                    continue;
                }
                if (body->kind == BaseCelestialBody::Kind::Star) {
                    hasStar = true;
                }
                const SourceScopedBody scoped{*body, source.sourceId};
                if (hasAnyMatch(index, scoped.body())) {
                    continue;
                }
                const std::size_t position = accumulator.append(scoped.body(), source.sourceId);
                index.add(scoped.body(), position);
            }
            continue;
        }

        MergeAccumulator sourceBodies;
        CatalogIdentityIndex sourceIndex;
        for (const BaseCelestialBody* body : source.catalog->bodies()) {
            if (body == nullptr || !participates(source.policy, *body)) {
                continue;
            }
            if (body->kind == BaseCelestialBody::Kind::Star) {
                hasStar = true;
            }
            appendDeduped(sourceBodies, sourceIndex, *body, source.sourceId);
        }

        const CatalogIdentityIndex activeIndex = buildActiveIndex(accumulator);
        for (std::size_t position = 0; position < sourceBodies.sourceIds.size(); ++position) {
            const BaseCelestialBody& body = sourceBodies.at(position);
            const MatchDecision decision = evaluateMatch(accumulator, activeIndex, body);
            if (decision.action == MatchDecision::Action::Merge) {
                const BaseCelestialBody& loser = accumulator.at(decision.matchIndex);
                std::vector<std::string> priorContributors = accumulator.contributorSourceIds[decision.matchIndex];
                bool conflict = false;
                if (body.kind == BaseCelestialBody::Kind::DeepSkyObject) {
                    DistantCelestialBody winner = CelestialBodyCatalog::copyDistantBody(body);
                    mergeSurvivorInPlace(winner, loser, conflict);
                    accumulator.vacate(decision.matchIndex);
                    if (conflict) {
                        logConflictingAstrometry(winner, loser);
                    }
                    static_cast<void>(accumulator.append(winner, source.sourceId, std::move(priorContributors)));
                } else {
                    OwnGalaxyCelestialBody winner = CelestialBodyCatalog::copyOwnGalaxyBody(body);
                    mergeSurvivorInPlace(winner, loser, conflict);
                    accumulator.vacate(decision.matchIndex);
                    if (conflict) {
                        logConflictingAstrometry(winner, loser);
                    }
                    static_cast<void>(accumulator.append(winner, source.sourceId, std::move(priorContributors)));
                }
                continue;
            }
            static_cast<void>(accumulator.append(body, source.sourceId));
        }
    }

    if (augmentCoreEnabled && !hasStar) {
        CatalogIdentityIndex index = buildActiveIndex(accumulator);
        for (const OwnGalaxyCelestialBody& brightStar : CoreBodyCatalogAugmenter::bundledBrightStars()) {
            if (hasAnyMatch(index, brightStar)) {
                continue;
            }
            const std::size_t position = accumulator.append(brightStar, augmentCoreSourceId);
            index.add(brightStar, position);
        }
    }

    CatalogCompositionMergeResult result;
    assembleResult(result, accumulator);
    return result;
}
}  // namespace skygate::ephemeris
