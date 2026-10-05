#include "CatalogCompositionMerger.hpp"

#include "StringUtilities.hpp"
#include "catalog/CatalogCoordinateModel.hpp"
#include "catalog/CatalogIdentifier.hpp"
#include "catalog/CatalogIdentityIndex.hpp"
#include "catalog/CatalogObjectIdentity.hpp"
#include "catalog/composition/CoreBodyCatalogAugmenter.hpp"

#include <QLoggingCategory>
#include <QString>
#include <QStringList>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
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

    // Appends a body owned by `sourceId`. The owning source heads the
    // contributor list, followed by the prior contributors in the descending
    // source precedence order the caller established. Prior ids are
    // deduplicated, so a source absorbed through several paths is listed once.
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

// Configured precedence of every source in a composition collection, keyed by
// source instance id. A source's rank is its position in the configured
// collection: a later source outranks every earlier source, exactly like a
// replacement winner outranks the survivor it absorbs. The rank is never
// derived from the accumulator position of a survivor, because replacement and
// bridge absorption vacate accumulator positions and append the winner at a
// new position, so position order stops matching source precedence after the
// first replacement.
class SourcePrecedence final {
public:
    void add(const std::string_view sourceId, const std::size_t rank)
    {
        m_ranks.insert_or_assign(std::string{sourceId}, rank);
    }

    [[nodiscard]] std::size_t rankOf(const std::string_view sourceId) const
    {
        const auto found = m_ranks.find(std::string{sourceId});
        Q_ASSERT(found != m_ranks.end());
        return found != m_ranks.end() ? found->second : 0U;
    }

    [[nodiscard]] bool outranks(const std::string_view lhsSourceId, const std::string_view rhsSourceId) const
    {
        return rankOf(lhsSourceId) > rankOf(rhsSourceId);
    }

private:
    std::unordered_map<std::string, std::size_t> m_ranks;
};

struct MatchDecision final {
    enum class Action {
        Append,
        Merge,
        Bridge
    };

    Action action = Action::Append;
    // Position of the single matched survivor; meaningful when action is Merge.
    std::size_t matchIndex = 0;
    // Same-kind survivors an ambiguous authoritative match bridged; non-empty
    // only when action is Bridge.
    std::vector<std::size_t> bridgeCandidates;
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

// Retains one authoritative canonical key of an absorbed body on the winner.
// The winner's own public id and keys already retained are skipped, and keys
// are compared case-insensitively like every other identity key. Retained keys
// are canonical identity data, never display aliases, so a row carrying one
// still resolves authoritatively to the survivor.
void retainCanonicalId(
    CatalogObjectIdentity& winner, const std::string_view winnerId, const std::string_view retainedId
)
{
    const std::string_view trimmed = StringUtilities::trimAsciiWhitespace(retainedId);
    if (trimmed.empty() || StringUtilities::equalsIgnoreAsciiCase(trimmed, winnerId)) {
        return;
    }
    StringUtilities::appendUniqueIgnoreAsciiCase(winner.retainedCanonicalIds, std::string{trimmed});
}

// Every canonical key the absorbed body was known by stays equivalent to the
// winner: its own public canonical id and every canonical id it had already
// retained. The equivalence therefore survives replacement and bridge chains,
// later compositions, and snapshots serialized after a composition.
void retainCanonicalEquivalences(
    CatalogObjectIdentity& winner, const std::string_view winnerId, const BaseCelestialBody& loser
)
{
    retainCanonicalId(winner, winnerId, loser.id);
    for (const std::string& retainedId : loser.identity.retainedCanonicalIds) {
        retainCanonicalId(winner, winnerId, retainedId);
    }
}

// Namespaces whose canonical deep-sky ids are recognized designations:
// ngc_<n>, ic_<n>, and messier_<nnn>.
constexpr std::string_view kDeepSkyDesignationNamespaces[] = {"ngc", "ic", "messier"};

// The designations a record authoritatively carries. Explicit external
// identifiers come first; a recognized deep-sky canonical id contributes the
// designation it was derived from, because the identity contract treats that
// id as a global identity even when a producer supplied no matching external
// identifier.
std::vector<CatalogIdentifier> authoritativeDesignations(const BaseCelestialBody& body)
{
    std::vector<CatalogIdentifier> designations;
    designations.reserve(body.identity.externalIdentifiers.size() + 1U);
    for (const CatalogIdentifier& identifier : body.identity.externalIdentifiers) {
        if (!identifier.empty()) {
            designations.push_back(identifier);
        }
    }

    for (const std::string_view designationNamespace : kDeepSkyDesignationNamespaces) {
        const std::string prefix = std::string(designationNamespace) + "_";
        if (!body.id.starts_with(prefix)) {
            continue;
        }

        CatalogIdentifier designation =
            CatalogIdentifier::make(std::string(designationNamespace), body.id.substr(prefix.size()));
        if (std::find(designations.begin(), designations.end(), designation) == designations.end()) {
            designations.push_back(std::move(designation));
        }
        break;
    }
    return designations;
}

// True when two records disagree about an authoritative namespace: both claim
// at least one value for the namespace but their value sets do not overlap.
// An overlapping value would already have been resolved authoritatively, so
// this only distinguishes otherwise non-authoritative alias matches. Only
// explicit cross-identifications may bridge two conflicting recognized
// designations; a shared descriptive name must keep them distinct.
bool hasContradictoryIdentifiers(const BaseCelestialBody& lhs, const BaseCelestialBody& rhs)
{
    const std::vector<CatalogIdentifier> lhsDesignations = authoritativeDesignations(lhs);
    const std::vector<CatalogIdentifier> rhsDesignations = authoritativeDesignations(rhs);

    std::vector<std::string_view> checkedNamespaces;
    for (const CatalogIdentifier& lhsIdentifier : lhsDesignations) {
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
        for (const CatalogIdentifier& identifier : lhsDesignations) {
            if (!identifier.empty() && identifier.namespaceName == lhsIdentifier.namespaceName) {
                lhsValues.push_back(identifier.value);
            }
        }
        for (const CatalogIdentifier& identifier : rhsDesignations) {
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

void logFixedCoordinateConflict(const BaseCelestialBody& winner, const BaseCelestialBody& loser)
{
    const QString message =
        QStringLiteral(
            "Catalog composition kept the fixed coordinates of %1 over conflicting fixed coordinates from %2."
        )
            .arg(bodyLabel(winner), bodyLabel(loser));
    qCWarning(skygateCatalogCompositionLog).noquote() << message;
}

void logRejectedAstrometry(const BaseCelestialBody& winner, const BaseCelestialBody& loser, const QString& keptModel)
{
    const QString message =
        QStringLiteral("Catalog composition kept the %1 of %2 and rejected the incompatible astrometry of %3.")
            .arg(keptModel, bodyLabel(winner), bodyLabel(loser));
    qCWarning(skygateCatalogCompositionLog).noquote() << message;
}

void logRejectedFixedCoordinates(const BaseCelestialBody& winner, const BaseCelestialBody& loser)
{
    const QString message = QStringLiteral(
                                "Catalog composition kept the reference coordinates of %1 and rejected the "
                                "incompatible fixed coordinates of %2."
    )
                                .arg(bodyLabel(winner), bodyLabel(loser));
    qCWarning(skygateCatalogCompositionLog).noquote() << message;
}

// Merges the losing fixed position into the winning coordinate model. The
// winner's own fixed position always wins; a losing fixed position only fills
// a missing winner position, and it is rejected when the winner's astrometry
// already anchors the object at a different direction. A fixed position
// carries no reference epoch, so the comparison against a reference position
// has no epoch conversion to apply.
void mergeFixedEquatorialInPlace(OwnGalaxyCelestialBody& winner, const BaseCelestialBody& loser)
{
    const skygate::core::EquatorialCoordinate* loserFixed = loser.fixedEquatorialCoordinate();
    if (loserFixed == nullptr) {
        return;
    }

    if (!winner.fixedEquatorial.has_value()) {
        if (winner.starAstrometry.has_value()
            && !CatalogCoordinateModel::sameDirection(winner.starAstrometry->referenceEquatorial, *loserFixed)) {
            logRejectedFixedCoordinates(winner, loser);
            return;
        }
        winner.fixedEquatorial = *loserFixed;
        return;
    }

    if (coordinatesConflict(*winner.fixedEquatorial, *loserFixed)) {
        logFixedCoordinateConflict(winner, loser);
    }
}

// Merges the losing astrometry into the winning coordinate model. The
// winner's reference position and reference epoch are authoritative. A losing
// astrometry only enters the record when it agrees with the surviving model:
// directly against a fixed position, or against the winning reference
// position after the losing proper motion accounts for the reference-epoch
// difference. Compatible losing astrometry fills only the fields the winner
// is missing; incompatible astrometry is rejected and diagnosed instead of
// producing a record with two contradictory coordinate descriptions.
void mergeStarAstrometryInPlace(OwnGalaxyCelestialBody& winner, const BaseCelestialBody& loser)
{
    const CatalogStarAstrometry* loserAstrometry = loser.catalogStarAstrometry();
    if (loserAstrometry == nullptr) {
        return;
    }

    if (!winner.starAstrometry.has_value()) {
        if (winner.fixedEquatorial.has_value()
            && !CatalogCoordinateModel::sameDirection(*winner.fixedEquatorial, loserAstrometry->referenceEquatorial)) {
            logRejectedAstrometry(winner, loser, QStringLiteral("fixed coordinates"));
            return;
        }
        winner.starAstrometry = *loserAstrometry;
        return;
    }

    if (!CatalogCoordinateModel::sameAstrometry(*winner.starAstrometry, *loserAstrometry)) {
        logRejectedAstrometry(winner, loser, QStringLiteral("reference coordinates"));
        return;
    }

    CatalogStarAstrometry& merged = *winner.starAstrometry;
    if (!merged.properMotionRightAscensionMasPerYear.has_value()) {
        merged.properMotionRightAscensionMasPerYear = loserAstrometry->properMotionRightAscensionMasPerYear;
    }
    if (!merged.properMotionDeclinationMasPerYear.has_value()) {
        merged.properMotionDeclinationMasPerYear = loserAstrometry->properMotionDeclinationMasPerYear;
    }
    if (!merged.stellarParallaxMas.has_value()) {
        merged.stellarParallaxMas = loserAstrometry->stellarParallaxMas;
    }
    if (!merged.radialVelocityKmPerSecond.has_value()) {
        merged.radialVelocityKmPerSecond = loserAstrometry->radialVelocityKmPerSecond;
    }
    if (!merged.validityRange.has_value()) {
        merged.validityRange = loserAstrometry->validityRange;
    }
}

void mergeOwnGalaxyInPlace(OwnGalaxyCelestialBody& winner, const BaseCelestialBody& loser)
{
    mergeIdentityInto(winner.identity, loser.identity);
    if (winner.displayName.empty() && !loser.displayName.empty()) {
        winner.displayName = loser.displayName;
    }

    mergeFixedEquatorialInPlace(winner, loser);
    mergeStarAstrometryInPlace(winner, loser);
}

void mergeDistantInPlace(DistantCelestialBody& winner, const BaseCelestialBody& loser)
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
        logFixedCoordinateConflict(winner, loser);
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

void mergeSurvivorInPlace(BaseCelestialBody& winner, const BaseCelestialBody& loser)
{
    retainCanonicalEquivalences(winner.identity, winner.id, loser);
    if (winner.kind == BaseCelestialBody::Kind::DeepSkyObject) {
        mergeDistantInPlace(static_cast<DistantCelestialBody&>(winner), loser);
        return;
    }
    mergeOwnGalaxyInPlace(static_cast<OwnGalaxyCelestialBody&>(winner), loser);
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

// Contributor source ids of the absorbed positions, ordered by configured
// source precedence with the highest-precedence contributor first. Callers
// prepend the winner's own source, which outranks every absorbed contributor,
// so the result stays in descending precedence order. Each contributing source
// is listed once even when several absorbed survivors name it. Accumulator
// position order is deliberately not used: earlier replacements leave
// contributor lists that interleave source ranks.
std::vector<std::string> collectContributors(
    const MergeAccumulator& accumulator,
    const std::vector<std::size_t>& positions,
    const SourcePrecedence& sourcePrecedence
)
{
    std::vector<std::string> contributors;
    for (const std::size_t position : positions) {
        for (const std::string& sourceId : accumulator.contributorSourceIds[position]) {
            if (std::find(contributors.begin(), contributors.end(), sourceId) == contributors.end()) {
                contributors.push_back(sourceId);
            }
        }
    }

    std::sort(
        contributors.begin(), contributors.end(), [&sourcePrecedence](const std::string& lhs, const std::string& rhs) {
            return sourcePrecedence.outranks(lhs, rhs);
        }
    );
    return contributors;
}

// Merges every absorbed survivor into `winner` in configured source precedence
// order, highest precedence first, so every missing winner value is filled
// from the highest-priority contributor that supplies it. Absorbed survivors
// of one source keep their accumulator order, which is the documented
// within-source row order: the first row of a source is authoritative and its
// later rows only fill it. Each merge diagnoses the coordinate or metadata
// conflicts the winner overrides.
void absorbSurvivors(
    BaseCelestialBody& winner,
    const MergeAccumulator& accumulator,
    std::vector<std::size_t> positions,
    const SourcePrecedence& sourcePrecedence
)
{
    std::stable_sort(
        positions.begin(),
        positions.end(),
        [&accumulator, &sourcePrecedence](const std::size_t lhs, const std::size_t rhs) {
            return sourcePrecedence.outranks(accumulator.sourceIds[lhs], accumulator.sourceIds[rhs]);
        }
    );

    for (const std::size_t position : positions) {
        mergeSurvivorInPlace(winner, accumulator.at(position));
    }
}

// Appends `incoming` as the survivor of every absorbed position. The incoming
// record wins because it is the later record that establishes the shared
// authoritative identity or replaces an earlier source's survivor. Absorbed
// positions are vacated and removed from the index before the winner is
// registered, so later rows resolve to the winner and never to a vacated body.
// Absorption retains each absorbed body's canonical id and canonical
// equivalences on the winner, so the earlier keys keep resolving to the
// survivor. Missing metadata and the contributor id order follow configured
// source precedence rather than accumulator position order.
std::size_t absorbSurvivorsAndAppend(
    MergeAccumulator& accumulator,
    CatalogIdentityIndex& index,
    const BaseCelestialBody& incoming,
    const std::vector<std::size_t>& absorbedPositions,
    std::string sourceId,
    const SourcePrecedence& sourcePrecedence
)
{
    std::vector<std::string> priorContributors = collectContributors(accumulator, absorbedPositions, sourcePrecedence);
    std::size_t position = 0;
    if (incoming.kind == BaseCelestialBody::Kind::DeepSkyObject) {
        DistantCelestialBody winner = CelestialBodyCatalog::copyDistantBody(incoming);
        absorbSurvivors(winner, accumulator, absorbedPositions, sourcePrecedence);
        for (const std::size_t absorbed : absorbedPositions) {
            accumulator.vacate(absorbed);
            index.remove(absorbed);
        }
        position = accumulator.append(winner, std::move(sourceId), std::move(priorContributors));
    } else {
        OwnGalaxyCelestialBody winner = CelestialBodyCatalog::copyOwnGalaxyBody(incoming);
        absorbSurvivors(winner, accumulator, absorbedPositions, sourcePrecedence);
        for (const std::size_t absorbed : absorbedPositions) {
            accumulator.vacate(absorbed);
            index.remove(absorbed);
        }
        position = accumulator.append(winner, std::move(sourceId), std::move(priorContributors));
    }
    index.add(accumulator.at(position), position);
    return position;
}

// Decides how `incoming` resolves against the bodies already indexed, logging
// every keep-distinct decision. An ambiguous authoritative match of the same
// kind is a bridge: the incoming record becomes the survivor of all matched
// same-kind bodies. Matched bodies of an incompatible kind stay distinct and
// are diagnosed.
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
            && hasContradictoryIdentifiers(existing, incoming)) {
            logAmbiguousAlias(existing, incoming);
            return decision;
        }

        decision.action = MatchDecision::Action::Merge;
        return decision;
    }

    if (resolution.kind == CatalogIdentityIndex::Resolution::MatchKind::AmbiguousAuthoritative) {
        for (const std::size_t candidate : resolution.candidates) {
            const BaseCelestialBody& existing = accumulator.at(candidate);
            if (existing.kind == incoming.kind) {
                decision.bridgeCandidates.push_back(candidate);
                continue;
            }
            logIncompatibleKind(existing, incoming);
        }
        if (!decision.bridgeCandidates.empty()) {
            decision.action = MatchDecision::Action::Bridge;
        }
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
    const std::string_view sourceId,
    const SourcePrecedence& sourcePrecedence
)
{
    const SourceScopedBody scoped{incoming, sourceId};
    const BaseCelestialBody& body = scoped.body();
    const MatchDecision decision = evaluateMatch(accumulator, index, body);
    if (decision.action == MatchDecision::Action::Merge) {
        mergeSurvivorInPlace(accumulator.at(decision.matchIndex), body);
        // Registers the merged record's keys as well, so a later record with
        // the same record key resolves to this survivor.
        index.add(body, decision.matchIndex);
        return decision.matchIndex;
    }

    if (decision.action == MatchDecision::Action::Bridge) {
        return absorbSurvivorsAndAppend(
            accumulator, index, body, decision.bridgeCandidates, std::string(sourceId), sourcePrecedence
        );
    }

    const std::size_t position = accumulator.append(body, std::string(sourceId));
    index.add(body, position);
    return position;
}

bool participates(const CatalogCompositionPolicy policy, const BaseCelestialBody& body)
{
    switch (policy) {
    case CatalogCompositionPolicy::DeepSkyOnly:
        return body.kind == BaseCelestialBody::Kind::DeepSkyObject;
    case CatalogCompositionPolicy::AugmentCore:
        return body.kind != BaseCelestialBody::Kind::DeepSkyObject;
    case CatalogCompositionPolicy::DeepSkyFallback:
        return body.kind == BaseCelestialBody::Kind::DeepSkyObject;
    case CatalogCompositionPolicy::Merge:
        return true;
    }
    return true;
}

// Gap-fill policies never replace an existing survivor: a participating body is
// appended only when no active body already claims its identity. AugmentCore
// and DeepSkyFallback differ in which bodies they select, not in precedence.
bool isGapFillPolicy(const CatalogCompositionPolicy policy)
{
    return policy == CatalogCompositionPolicy::AugmentCore || policy == CatalogCompositionPolicy::DeepSkyFallback;
}

bool hasAnyMatch(const CatalogIdentityIndex& index, const BaseCelestialBody& body)
{
    const CatalogIdentityIndex::Resolution resolution = index.resolve(body);
    return resolution.hasSingleMatch() || resolution.isAmbiguous();
}

// Positions in `positions` that share their object kind with another position.
// A shared authoritative identity across incompatible kinds is a deliberate,
// already diagnosed conflict; the same identity across survivors of one kind
// is an internal merge error.
std::vector<std::size_t>
sameKindDuplicates(const MergeAccumulator& accumulator, const std::vector<std::size_t>& positions)
{
    std::vector<std::size_t> duplicates;
    for (const std::size_t position : positions) {
        const BaseCelestialBody::Kind kind = accumulator.at(position).kind;
        const bool sharesKind = std::any_of(positions.begin(), positions.end(), [&](const std::size_t other) {
            return other != position && accumulator.at(other).kind == kind;
        });
        if (sharesKind) {
            duplicates.push_back(position);
        }
    }
    return duplicates;
}

// Merge validation: no authoritative identity may be shared by two active
// survivors of the same kind. The validated index is rebuilt from the active
// survivors rather than reused from the merge, so the check also catches an
// index that fell out of step with the merged collection.
void logUnexpectedDuplicateIdentities(const MergeAccumulator& accumulator)
{
    CatalogIdentityIndex validationIndex;
    validationIndex.reserve(accumulator.active.size());
    for (std::size_t position = 0; position < accumulator.active.size(); ++position) {
        if (accumulator.active[position]) {
            validationIndex.add(accumulator.at(position), position);
        }
    }

    for (const CatalogIdentityIndex::DuplicateIdentity& duplicate :
         validationIndex.duplicateAuthoritativeIdentities()) {
        const std::vector<std::size_t> conflicting = sameKindDuplicates(accumulator, duplicate.positions);
        if (conflicting.empty()) {
            continue;
        }

        QStringList labels;
        labels.reserve(static_cast<qsizetype>(conflicting.size()));
        for (const std::size_t position : conflicting) {
            labels.append(bodyLabel(accumulator.at(position)));
        }

        const QString message = QStringLiteral(
                                    "Catalog composition validation found unexpected duplicate authoritative "
                                    "identity %1 across %2 active survivors of the same kind: %3."
        )
                                    .arg(QString::fromStdString(duplicate.key))
                                    .arg(static_cast<qulonglong>(conflicting.size()))
                                    .arg(labels.join(QStringLiteral(", ")));
        qCWarning(skygateCatalogCompositionLog).noquote() << message;
    }
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
    CatalogIdentityIndex activeIndex;
    // Rank every configured source by its collection position before merging:
    // a later source has higher precedence than every earlier one. Disabled
    // sources are ranked too so a source id lookup is total, but they never
    // contribute bodies or metadata.
    SourcePrecedence sourcePrecedence;
    for (std::size_t index = 0; index < request.sources.size(); ++index) {
        sourcePrecedence.add(request.sources[index].sourceId, index);
    }

    bool augmentCoreEnabled = false;
    std::string augmentCoreSourceId;
    bool hasStar = false;

    for (const CatalogCompositionSourceEntry& source : request.sources) {
        if (!source.enabled || source.catalog == nullptr) {
            continue;
        }

        // Gap-fill sources contribute only the bodies their policy selects and
        // never replace an earlier survivor. AugmentCore additionally enables
        // the bundled bright-star fallback when no other source supplies a
        // star; DeepSkyFallback fills missing deep-sky identities only.
        if (isGapFillPolicy(source.policy)) {
            if (source.policy == CatalogCompositionPolicy::AugmentCore) {
                augmentCoreEnabled = true;
                if (augmentCoreSourceId.empty()) {
                    augmentCoreSourceId = source.sourceId;
                }
            }

            for (const BaseCelestialBody* body : source.catalog->bodies()) {
                if (body == nullptr || !participates(source.policy, *body)) {
                    continue;
                }
                if (body->kind == BaseCelestialBody::Kind::Star) {
                    hasStar = true;
                }
                const SourceScopedBody scoped{*body, source.sourceId};
                if (hasAnyMatch(activeIndex, scoped.body())) {
                    continue;
                }
                const std::size_t position = accumulator.append(scoped.body(), source.sourceId);
                activeIndex.add(accumulator.at(position), position);
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
            appendDeduped(sourceBodies, sourceIndex, *body, source.sourceId, sourcePrecedence);
        }

        // Positions from this pass start here, so a match at or above this
        // boundary belongs to the source currently being merged.
        const std::size_t passStartPosition = accumulator.sourceIds.size();
        for (std::size_t position = 0; position < sourceBodies.sourceIds.size(); ++position) {
            if (!sourceBodies.active[position]) {
                continue;
            }

            const BaseCelestialBody& body = sourceBodies.at(position);
            const MatchDecision decision = evaluateMatch(accumulator, activeIndex, body);
            if (decision.action == MatchDecision::Action::Merge) {
                if (decision.matchIndex >= passStartPosition) {
                    // This pass already produced the survivor: the first record
                    // of a source is authoritative and later records of the
                    // same pass only fill its missing metadata.
                    mergeSurvivorInPlace(accumulator.at(decision.matchIndex), body);
                    activeIndex.add(body, decision.matchIndex);
                    continue;
                }

                // A later source wins over an earlier source's survivor and
                // absorbs its non-conflicting identity and metadata.
                static_cast<void>(absorbSurvivorsAndAppend(
                    accumulator, activeIndex, body, {decision.matchIndex}, source.sourceId, sourcePrecedence
                ));
                continue;
            }

            if (decision.action == MatchDecision::Action::Bridge) {
                static_cast<void>(absorbSurvivorsAndAppend(
                    accumulator, activeIndex, body, decision.bridgeCandidates, source.sourceId, sourcePrecedence
                ));
                continue;
            }

            const std::size_t appendedPosition = accumulator.append(body, source.sourceId);
            activeIndex.add(accumulator.at(appendedPosition), appendedPosition);
        }
    }

    if (augmentCoreEnabled && !hasStar) {
        for (const OwnGalaxyCelestialBody& brightStar : CoreBodyCatalogAugmenter::bundledBrightStars()) {
            if (hasAnyMatch(activeIndex, brightStar)) {
                continue;
            }
            const std::size_t position = accumulator.append(brightStar, augmentCoreSourceId);
            activeIndex.add(accumulator.at(position), position);
        }
    }

    logUnexpectedDuplicateIdentities(accumulator);

    CatalogCompositionMergeResult result;
    assembleResult(result, accumulator);
    return result;
}
}  // namespace skygate::ephemeris
