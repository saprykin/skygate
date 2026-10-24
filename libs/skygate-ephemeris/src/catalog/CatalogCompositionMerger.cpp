#include "CatalogCompositionMerger.hpp"

#include "catalog/CatalogCompositionLogging.hpp"
#include "catalog/CatalogCompositionPolicyRules.hpp"
#include "catalog/CatalogCoordinateMerge.hpp"
#include "catalog/CatalogFieldOrigins.hpp"
#include "catalog/CatalogIdentityAbsorption.hpp"
#include "catalog/CatalogIdentityIndex.hpp"
#include "catalog/CatalogMergeAccumulator.hpp"
#include "catalog/CatalogObjectIdentity.hpp"
#include "catalog/CatalogSourcePrecedence.hpp"
#include "catalog/composition/CoreBodyCatalogAugmenter.hpp"

#include <QLoggingCategory>
#include <QString>
#include <QStringList>
#include <QtGlobal>

#include <algorithm>
#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace skygate::ephemeris {
namespace {

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

void logIncompatibleKind(const BaseCelestialBody& existing, const BaseCelestialBody& incoming)
{
    qCWarning(skygateCatalogCompositionLog).noquote()
        << "Catalog composition kept" << CatalogCoordinateMerge::bodyLabel(incoming) << "distinct from"
        << CatalogCoordinateMerge::bodyLabel(existing)
        << "because the shared identity is used by incompatible object kinds.";
}

void logAmbiguousAlias(const BaseCelestialBody& existing, const BaseCelestialBody& incoming)
{
    qCWarning(skygateCatalogCompositionLog).noquote()
        << "Catalog composition kept" << CatalogCoordinateMerge::bodyLabel(incoming) << "distinct from"
        << CatalogCoordinateMerge::bodyLabel(existing)
        << "because their shared alias is ambiguous across authoritative identifiers.";
}

void logAmbiguousResolution(const CatalogIdentityIndex::Resolution& resolution, const BaseCelestialBody& incoming)
{
    qCWarning(skygateCatalogCompositionLog).noquote()
        << "Catalog composition kept" << CatalogCoordinateMerge::bodyLabel(incoming)
        << "distinct because its identity matched" << resolution.candidates.size() << "different bodies.";
}

// Decides how `incoming` resolves against the bodies already indexed, logging
// every keep-distinct decision. An ambiguous authoritative match of the same
// kind is a bridge: the incoming record becomes the survivor of all matched
// same-kind bodies. Matched bodies of an incompatible kind stay distinct and
// are diagnosed.
MatchDecision evaluateMatch(
    const CatalogMergeAccumulator& accumulator, const CatalogIdentityIndex& index, const BaseCelestialBody& incoming
)
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
            && CatalogIdentityAbsorption::hasContradictoryIdentifiers(existing, incoming)) {
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
// sources never match. `rowOrdinal` is the position of the record in the
// source, so every value it supplies keeps its true row origin.
std::size_t appendDeduped(
    CatalogMergeAccumulator& accumulator,
    CatalogIdentityIndex& index,
    const BaseCelestialBody& incoming,
    const std::string_view sourceId,
    const CatalogSourcePrecedence& sourcePrecedence,
    const std::size_t rowOrdinal
)
{
    const SourceScopedBody scoped{incoming, sourceId};
    const BaseCelestialBody& body = scoped.body();
    const std::size_t sourceRank = sourcePrecedence.rankOf(sourceId);
    const MatchDecision decision = evaluateMatch(accumulator, index, body);
    if (decision.action == MatchDecision::Action::Merge) {
        CatalogIdentityAbsorption::mergeSurvivorInPlace(
            accumulator.at(decision.matchIndex),
            accumulator.fieldOrigins[decision.matchIndex],
            body,
            CatalogFieldOrigins::originsOf(body, sourceRank, rowOrdinal),
            CatalogCoordinateMerge::ValuePrecedence::KeepWinnerValue
        );
        // Registers the merged record's keys as well, so a later record with
        // the same record key resolves to this survivor.
        index.add(body, decision.matchIndex);
        return decision.matchIndex;
    }

    if (decision.action == MatchDecision::Action::Bridge) {
        return CatalogIdentityAbsorption::absorbSurvivorsAndAppend(
            accumulator,
            index,
            body,
            CatalogFieldOrigins::originsOf(body, sourceRank, rowOrdinal),
            decision.bridgeCandidates,
            std::string(sourceId),
            sourcePrecedence
        );
    }

    const std::size_t position =
        accumulator.append(body, std::string(sourceId), CatalogFieldOrigins::originsOf(body, sourceRank, rowOrdinal));
    index.add(body, position);
    return position;
}

// Applies the shared identity decision to one body of a gap-fill source and
// appends it only when its identity is not already established. A proven
// equivalence keeps the earlier survivor untouched; a contradicted or
// ambiguous weak alias match appends the body as an independent object with
// the normal keep-distinct diagnostics. Only replacement-versus-preservation
// is policy-specific, never the identity decision itself.
void appendGapFillBody(
    CatalogMergeAccumulator& accumulator,
    CatalogIdentityIndex& index,
    const BaseCelestialBody& body,
    const std::string_view sourceId,
    const CatalogSourcePrecedence& sourcePrecedence,
    const std::size_t rowOrdinal
)
{
    const MatchDecision decision = evaluateMatch(accumulator, index, body);
    if (decision.action != MatchDecision::Action::Append) {
        return;
    }

    const std::size_t position = accumulator.append(
        body, std::string(sourceId), CatalogFieldOrigins::originsOf(body, sourcePrecedence.rankOf(sourceId), rowOrdinal)
    );
    index.add(accumulator.at(position), position);
}

// Positions in `positions` that share their object kind with another position.
// A shared authoritative identity across incompatible kinds is a deliberate,
// already diagnosed conflict; the same identity across survivors of one kind
// is an internal merge error.
std::vector<std::size_t>
sameKindDuplicates(const CatalogMergeAccumulator& accumulator, const std::vector<std::size_t>& positions)
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
void logUnexpectedDuplicateIdentities(const CatalogMergeAccumulator& accumulator)
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
            labels.append(CatalogCoordinateMerge::bodyLabel(accumulator.at(position)));
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

void assembleResult(CatalogCompositionMergeResult& result, const CatalogMergeAccumulator& accumulator)
{
    result.ownGalaxyBodies.reserve(accumulator.ownGalaxyBodies.size());
    result.distantBodies.reserve(accumulator.distantBodies.size());
    result.orderedBodyIndexes.reserve(accumulator.sourceIds.size());
    result.sourceIds.reserve(accumulator.sourceIds.size());
    result.contributorSourceIds.reserve(accumulator.sourceIds.size());

    // Both domains append the same way: the order entry points at the body's
    // position in its own domain vector, and the source bookkeeping stays
    // parallel to the order entries.
    const auto pushBody = [&result](
                              const auto& body,
                              auto& bodies,
                              const CelestialBodyCatalog::BodyDomain domain,
                              const std::string_view sourceId,
                              const std::vector<std::string>& contributorSourceIds
                          ) {
        result.orderedBodyIndexes.push_back(
            CelestialBodyCatalog::OrderEntry{.domain = domain, .bodyIndex = bodies.size()}
        );
        bodies.push_back(body);
        result.sourceIds.emplace_back(sourceId);
        result.contributorSourceIds.push_back(contributorSourceIds);
    };

    for (std::size_t position = 0; position < accumulator.sourceIds.size(); ++position) {
        if (!accumulator.active[position]) {
            continue;
        }

        const std::size_t bodyIndex = accumulator.domainIndex[position];
        if (accumulator.isDistant[position]) {
            pushBody(
                accumulator.distantBodies[bodyIndex],
                result.distantBodies,
                CelestialBodyCatalog::BodyDomain::Distant,
                accumulator.sourceIds[position],
                accumulator.contributorSourceIds[position]
            );
            continue;
        }

        pushBody(
            accumulator.ownGalaxyBodies[bodyIndex],
            result.ownGalaxyBodies,
            CelestialBodyCatalog::BodyDomain::OwnGalaxy,
            accumulator.sourceIds[position],
            accumulator.contributorSourceIds[position]
        );
    }
}

}  // namespace

CatalogCompositionMergeResult CatalogCompositionMerger::mergeCollection(const CatalogCompositionRequest& request)
{
    CatalogMergeAccumulator accumulator;
    CatalogIdentityIndex activeIndex;
    // Rank every configured source by its collection position before merging:
    // a later source has higher precedence than every earlier one. Disabled
    // sources are ranked too so a source id lookup is total, but they never
    // contribute bodies or metadata.
    CatalogSourcePrecedence sourcePrecedence;
    for (std::size_t index = 0; index < request.sources.size(); ++index) {
        sourcePrecedence.add(request.sources[index].sourceId, index);
    }

    bool augmentCoreEnabled = false;
    std::string augmentCoreSourceId;
    bool hasStar = false;
    // Monotonically increasing position of every processed record. It stays
    // true across sources, and within one source it is the row order that
    // resolves equal-rank value origins.
    std::size_t rowOrdinal = 0;

    for (const CatalogCompositionSourceEntry& source : request.sources) {
        if (!source.enabled || source.catalog == nullptr) {
            continue;
        }

        // Gap-fill sources contribute only the bodies their policy selects,
        // and the shared identity decision decides which of those are already
        // established. No gap-fill body ever replaces an earlier survivor.
        // AugmentCore additionally enables the bundled bright-star fallback
        // when no other source supplies a star; DeepSkyFallback fills missing
        // deep-sky identities only.
        if (CatalogCompositionPolicyRules::isGapFill(source.policy)) {
            if (CatalogCompositionPolicyRules::enablesBundledStarFallback(source.policy)) {
                augmentCoreEnabled = true;
                if (augmentCoreSourceId.empty()) {
                    augmentCoreSourceId = source.sourceId;
                }
            }

            for (const BaseCelestialBody* body : source.catalog->bodies()) {
                if (body == nullptr || !CatalogCompositionPolicyRules::participates(source.policy, *body)) {
                    continue;
                }
                if (body->kind == BaseCelestialBody::Kind::Star) {
                    hasStar = true;
                }
                const SourceScopedBody scoped{*body, source.sourceId};
                appendGapFillBody(
                    accumulator, activeIndex, scoped.body(), source.sourceId, sourcePrecedence, rowOrdinal++
                );
            }
            continue;
        }

        CatalogMergeAccumulator sourceBodies;
        CatalogIdentityIndex sourceIndex;
        for (const BaseCelestialBody* body : source.catalog->bodies()) {
            if (body == nullptr || !CatalogCompositionPolicyRules::participates(source.policy, *body)) {
                continue;
            }
            if (body->kind == BaseCelestialBody::Kind::Star) {
                hasStar = true;
            }
            appendDeduped(sourceBodies, sourceIndex, *body, source.sourceId, sourcePrecedence, rowOrdinal++);
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
                    // Both records belong to the current source: fill the
                    // survivor's missing values, keep the values it inherited
                    // from other sources, and resolve a value both records
                    // supply from this source by the earlier supplying row,
                    // because a bridge can append a survivor behind rows it
                    // outranks.
                    CatalogIdentityAbsorption::mergeSurvivorInPlace(
                        accumulator.at(decision.matchIndex),
                        accumulator.fieldOrigins[decision.matchIndex],
                        body,
                        sourceBodies.fieldOrigins[position],
                        CatalogCoordinateMerge::ValuePrecedence::TakeEarlierSameSourceRow
                    );
                    activeIndex.add(body, decision.matchIndex);
                    continue;
                }

                // A later source wins over an earlier source's survivor and
                // absorbs its non-conflicting identity and metadata.
                static_cast<void>(CatalogIdentityAbsorption::absorbSurvivorsAndAppend(
                    accumulator,
                    activeIndex,
                    body,
                    sourceBodies.fieldOrigins[position],
                    {decision.matchIndex},
                    source.sourceId,
                    sourcePrecedence
                ));
                continue;
            }

            if (decision.action == MatchDecision::Action::Bridge) {
                static_cast<void>(CatalogIdentityAbsorption::absorbSurvivorsAndAppend(
                    accumulator,
                    activeIndex,
                    body,
                    sourceBodies.fieldOrigins[position],
                    decision.bridgeCandidates,
                    source.sourceId,
                    sourcePrecedence
                ));
                continue;
            }

            const std::size_t appendedPosition =
                accumulator.append(body, source.sourceId, sourceBodies.fieldOrigins[position]);
            activeIndex.add(accumulator.at(appendedPosition), appendedPosition);
        }
    }

    if (augmentCoreEnabled && !hasStar) {
        for (const OwnGalaxyCelestialBody& brightStar : CoreBodyCatalogAugmenter::bundledBrightStars()) {
            appendGapFillBody(
                accumulator, activeIndex, brightStar, augmentCoreSourceId, sourcePrecedence, rowOrdinal++
            );
        }
    }

    logUnexpectedDuplicateIdentities(accumulator);

    CatalogCompositionMergeResult result;
    assembleResult(result, accumulator);
    return result;
}
}  // namespace skygate::ephemeris
