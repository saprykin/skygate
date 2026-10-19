#include "CatalogIdentityAbsorption.hpp"

#include "CelestialBodyCatalog.hpp"
#include "StringUtilities.hpp"
#include "catalog/CatalogCoordinateMerge.hpp"
#include "catalog/CatalogFieldOrigins.hpp"
#include "catalog/CatalogIdentifier.hpp"

#include <algorithm>
#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace skygate::ephemeris {

// Retains the union of identifiers from both records, including several
// values in the same namespace (a single object can carry multiple NGC or IC
// designations).
void CatalogIdentityAbsorption::mergeExternalIdentifiersInto(
    CatalogObjectIdentity& winner, const CatalogObjectIdentity& loser
)
{
    for (const CatalogIdentifier& identifier : loser.externalIdentifiers) {
        CatalogIdentifier::appendUnique(winner.externalIdentifiers, identifier);
    }
}

void CatalogIdentityAbsorption::mergeAliasesInto(
    std::vector<std::string>& winner, const std::vector<std::string>& loser
)
{
    for (const std::string& alias : loser) {
        StringUtilities::appendUniqueIgnoreAsciiCase(winner, alias);
    }
}

void CatalogIdentityAbsorption::mergeIdentityInto(CatalogObjectIdentity& winner, const CatalogObjectIdentity& loser)
{
    mergeExternalIdentifiersInto(winner, loser);
    mergeAliasesInto(winner.aliases, loser.aliases);
}

// Retains one authoritative canonical key of an absorbed body on the winner.
// The winner's own public id and keys already retained are skipped, and keys
// are compared case-insensitively like every other identity key. Retained keys
// are canonical identity data, never display aliases, so a row carrying one
// still resolves authoritatively to the survivor.
void CatalogIdentityAbsorption::retainCanonicalId(
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
void CatalogIdentityAbsorption::retainCanonicalEquivalences(
    CatalogObjectIdentity& winner, const std::string_view winnerId, const BaseCelestialBody& loser
)
{
    retainCanonicalId(winner, winnerId, loser.id);
    for (const std::string& retainedId : loser.identity.retainedCanonicalIds) {
        retainCanonicalId(winner, winnerId, retainedId);
    }
}

// The designations a record authoritatively carries. Explicit external
// identifiers come first; a recognized deep-sky canonical id contributes the
// designation it was derived from, because the identity contract treats that
// id as a global identity even when a producer supplied no matching external
// identifier.
std::vector<CatalogIdentifier> CatalogIdentityAbsorption::authoritativeDesignations(const BaseCelestialBody& body)
{
    std::vector<CatalogIdentifier> designations;
    designations.reserve(body.identity.externalIdentifiers.size() + 1U);
    for (const CatalogIdentifier& identifier : body.identity.externalIdentifiers) {
        CatalogIdentifier::appendUnique(designations, identifier);
    }

    if (const std::optional<CatalogIdentifier> designation = CatalogIdentifier::designationFromCanonicalId(body.id);
        designation.has_value()) {
        CatalogIdentifier::appendUnique(designations, *designation);
    }
    return designations;
}

// True when two records disagree about an authoritative namespace: both claim
// at least one value for the namespace but their value sets do not overlap.
// An overlapping value would already have been resolved authoritatively, so
// this only distinguishes otherwise non-authoritative alias matches. Only
// explicit cross-identifications may bridge two conflicting recognized
// designations; a shared descriptive name must keep them distinct.
bool CatalogIdentityAbsorption::hasContradictoryIdentifiers(const BaseCelestialBody& lhs, const BaseCelestialBody& rhs)
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

void CatalogIdentityAbsorption::mergeOwnGalaxyInPlace(
    OwnGalaxyCelestialBody& winner,
    CatalogFieldOrigins& winnerOrigins,
    const BaseCelestialBody& loser,
    const CatalogFieldOrigins& loserOrigins,
    const CatalogCoordinateMerge::ValuePrecedence precedence
)
{
    mergeIdentityInto(winner.identity, loser.identity);
    CatalogCoordinateMerge::takeHigherPrecedence(
        winner.displayName, winnerOrigins.displayName, loser.displayName, loserOrigins.displayName, precedence
    );
    CatalogCoordinateMerge::takeHigherPrecedence(
        winner.visualMagnitude,
        winnerOrigins.visualMagnitude,
        loser.visualMagnitude,
        loserOrigins.visualMagnitude,
        precedence
    );

    CatalogCoordinateMerge::mergeFixedEquatorialInPlace(winner, winnerOrigins, loser, loserOrigins, precedence);
    CatalogCoordinateMerge::mergeStarAstrometryInPlace(winner, winnerOrigins, loser, loserOrigins, precedence);
}

void CatalogIdentityAbsorption::mergeDistantInPlace(
    DistantCelestialBody& winner,
    CatalogFieldOrigins& winnerOrigins,
    const BaseCelestialBody& loser,
    const CatalogFieldOrigins& loserOrigins,
    const CatalogCoordinateMerge::ValuePrecedence precedence
)
{
    mergeIdentityInto(winner.identity, loser.identity);
    CatalogCoordinateMerge::takeHigherPrecedence(
        winner.displayName, winnerOrigins.displayName, loser.displayName, loserOrigins.displayName, precedence
    );
    CatalogCoordinateMerge::takeHigherPrecedence(
        winner.visualMagnitude,
        winnerOrigins.visualMagnitude,
        loser.visualMagnitude,
        loserOrigins.visualMagnitude,
        precedence
    );

    CatalogCoordinateMerge::mergeLosingFixedPosition(winner, winnerOrigins, loser, loserOrigins, precedence);

    const DeepSkyObjectInfo* loserDeepSky = loser.deepSkyObjectInfo();
    if (loserDeepSky == nullptr) {
        return;
    }
    if (!winner.deepSkyObject.has_value()) {
        winner.deepSkyObject = *loserDeepSky;
        winnerOrigins.deepSkyKind = loserOrigins.deepSkyKind;
        winnerOrigins.majorAxis = loserOrigins.majorAxis;
        winnerOrigins.minorAxis = loserOrigins.minorAxis;
        winnerOrigins.positionAngle = loserOrigins.positionAngle;
        return;
    }

    DeepSkyObjectInfo& merged = *winner.deepSkyObject;
    CatalogCoordinateMerge::takeHigherPrecedence(
        merged.kind, winnerOrigins.deepSkyKind, loserDeepSky->kind, loserOrigins.deepSkyKind, precedence
    );
    CatalogCoordinateMerge::takeHigherPrecedence(
        merged.majorAxisArcmin,
        winnerOrigins.majorAxis,
        loserDeepSky->majorAxisArcmin,
        loserOrigins.majorAxis,
        precedence
    );
    CatalogCoordinateMerge::takeHigherPrecedence(
        merged.minorAxisArcmin,
        winnerOrigins.minorAxis,
        loserDeepSky->minorAxisArcmin,
        loserOrigins.minorAxis,
        precedence
    );
    CatalogCoordinateMerge::takeHigherPrecedence(
        merged.positionAngleDeg,
        winnerOrigins.positionAngle,
        loserDeepSky->positionAngleDeg,
        loserOrigins.positionAngle,
        precedence
    );
    mergeAliasesInto(merged.aliases, loserDeepSky->aliases);
}

void CatalogIdentityAbsorption::mergeSurvivorInPlace(
    BaseCelestialBody& winner,
    CatalogFieldOrigins& winnerOrigins,
    const BaseCelestialBody& loser,
    const CatalogFieldOrigins& loserOrigins,
    const CatalogCoordinateMerge::ValuePrecedence precedence
)
{
    retainCanonicalEquivalences(winner.identity, winner.id, loser);
    if (winner.kind == BaseCelestialBody::Kind::DeepSkyObject) {
        mergeDistantInPlace(static_cast<DistantCelestialBody&>(winner), winnerOrigins, loser, loserOrigins, precedence);
        return;
    }
    mergeOwnGalaxyInPlace(static_cast<OwnGalaxyCelestialBody&>(winner), winnerOrigins, loser, loserOrigins, precedence);
}

// Contributor source ids of the absorbed positions, ordered by configured
// source precedence with the highest-precedence contributor first. Callers
// prepend the winner's own source, which outranks every absorbed contributor,
// so the result stays in descending precedence order. Each contributing source
// is listed once even when several absorbed survivors name it. Accumulator
// position order is deliberately not used: earlier replacements leave
// contributor lists that interleave source ranks.
std::vector<std::string> CatalogIdentityAbsorption::collectContributors(
    const CatalogMergeAccumulator& accumulator,
    const std::vector<std::size_t>& positions,
    const CatalogSourcePrecedence& sourcePrecedence
)
{
    std::vector<std::string> contributors;
    for (const std::size_t position : positions) {
        for (const std::string& sourceId : accumulator.contributorSourceIds[position]) {
            StringUtilities::appendUnique(contributors, sourceId);
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
// order, highest precedence first, and within one source in the order their
// coordinate models take precedence: a survivor without a model orders after
// every survivor that has one, and the earlier model anchor comes first. The
// order only decides processing and diagnostic order; which model wins comes
// from the origin comparisons. Each value is filled or replaced by the
// absorbed survivor whose value origin outranks the origin the winner carries:
// the source that actually supplied the value with the highest rank, or, when
// both values belong to one source, the earlier row of that source, because the
// first row of a source is authoritative and its later rows only fill it. So a
// value a lower-precedence source supplied through an intermediate survivor is
// never promoted to that survivor's rank, and a bridge across survivors of one
// source never outranks the rows that supplied their metadata. A loser whose
// coherent coordinate model anchor outranks the winner's replaces the whole
// model instead of being merged component by component. Each merge diagnoses
// the coordinate or metadata conflicts the winner overrides.
void CatalogIdentityAbsorption::absorbSurvivors(
    BaseCelestialBody& winner,
    CatalogFieldOrigins& winnerOrigins,
    const CatalogMergeAccumulator& accumulator,
    std::vector<std::size_t> positions,
    const CatalogSourcePrecedence& sourcePrecedence
)
{
    std::stable_sort(
        positions.begin(),
        positions.end(),
        [&accumulator, &sourcePrecedence](const std::size_t lhs, const std::size_t rhs) {
            if (sourcePrecedence.outranks(accumulator.sourceIds[lhs], accumulator.sourceIds[rhs])) {
                return true;
            }
            if (sourcePrecedence.outranks(accumulator.sourceIds[rhs], accumulator.sourceIds[lhs])) {
                return false;
            }
            const std::optional<CatalogFieldOrigins::ValueOrigin>& lhsModel =
                accumulator.fieldOrigins[lhs].coordinateModel;
            const std::optional<CatalogFieldOrigins::ValueOrigin>& rhsModel =
                accumulator.fieldOrigins[rhs].coordinateModel;
            if (lhsModel.has_value() != rhsModel.has_value()) {
                return lhsModel.has_value();
            }
            return lhsModel.has_value() && CatalogFieldOrigins::outranks(*lhsModel, *rhsModel);
        }
    );

    for (const std::size_t position : positions) {
        mergeSurvivorInPlace(
            winner,
            winnerOrigins,
            accumulator.at(position),
            accumulator.fieldOrigins[position],
            CatalogCoordinateMerge::ValuePrecedence::TakeHigherOrigin
        );
    }
}

// Appends `winner` as the survivor of every absorbed position. The absorbed
// survivors merge into `winner` under origin precedence, and their positions
// are vacated and removed from the index before the survivor is registered, so
// later rows resolve to the new survivor and never to a vacated body.
template <typename WinnerBody>
std::size_t CatalogIdentityAbsorption::appendAbsorbedSurvivor(
    CatalogMergeAccumulator& accumulator,
    CatalogIdentityIndex& index,
    WinnerBody winner,
    CatalogFieldOrigins winnerOrigins,
    const std::vector<std::size_t>& absorbedPositions,
    std::string sourceId,
    std::vector<std::string> priorContributors,
    const CatalogSourcePrecedence& sourcePrecedence
)
{
    absorbSurvivors(winner, winnerOrigins, accumulator, absorbedPositions, sourcePrecedence);
    for (const std::size_t absorbed : absorbedPositions) {
        accumulator.vacate(absorbed);
        index.remove(absorbed);
    }
    return accumulator.append(winner, std::move(sourceId), std::move(winnerOrigins), std::move(priorContributors));
}

// Appends `incoming` as the survivor of every absorbed position. The incoming
// record supplies the public canonical id because it is the later record that
// establishes the shared authoritative identity or replaces an earlier
// source's survivor. Its own values keep their origins and compete field by
// field with the absorbed survivors' values under origin precedence: a value
// an absorbed survivor supplied outranks the incoming value only when its
// source rank is higher or its row is earlier within the same source, so a
// later replacing source's value overrides the survivor it absorbs while a
// bridge over one source's own survivors never demotes the earlier supplying
// rows. Absorption retains each absorbed body's canonical id and canonical
// equivalences on the winner, so the earlier keys keep resolving to the
// survivor. A value the incoming record does not supply follows the origin
// precedence of the absorbed survivors that do, and the contributor id order
// follows configured source precedence rather than accumulator position
// order.
std::size_t CatalogIdentityAbsorption::absorbSurvivorsAndAppend(
    CatalogMergeAccumulator& accumulator,
    CatalogIdentityIndex& index,
    const BaseCelestialBody& incoming,
    CatalogFieldOrigins incomingOrigins,
    const std::vector<std::size_t>& absorbedPositions,
    std::string sourceId,
    const CatalogSourcePrecedence& sourcePrecedence
)
{
    std::vector<std::string> priorContributors = collectContributors(accumulator, absorbedPositions, sourcePrecedence);
    CatalogFieldOrigins winnerOrigins = std::move(incomingOrigins);
    // The incoming record decides which body domain owns the survivor; the
    // absorb and append bookkeeping is the same for both domains.
    std::size_t position = 0;
    if (incoming.kind == BaseCelestialBody::Kind::DeepSkyObject) {
        position = appendAbsorbedSurvivor(
            accumulator,
            index,
            CelestialBodyCatalog::copyDistantBody(incoming),
            std::move(winnerOrigins),
            absorbedPositions,
            std::move(sourceId),
            std::move(priorContributors),
            sourcePrecedence
        );
    } else {
        position = appendAbsorbedSurvivor(
            accumulator,
            index,
            CelestialBodyCatalog::copyOwnGalaxyBody(incoming),
            std::move(winnerOrigins),
            absorbedPositions,
            std::move(sourceId),
            std::move(priorContributors),
            sourcePrecedence
        );
    }
    index.add(accumulator.at(position), position);
    return position;
}

}  // namespace skygate::ephemeris
