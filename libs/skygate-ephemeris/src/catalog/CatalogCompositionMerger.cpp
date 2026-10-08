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
#include <QtGlobal>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
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

// Origin of one merged value: the rank of the source that supplied it and the
// row order of the supplying record within that source. A value of a
// higher-ranked source outranks a lower-ranked one; values that share a source
// rank resolve by row order, where the first row of a source is authoritative
// and its later rows only fill missing values.
struct ValueOrigin final {
    std::size_t sourceRank = 0;
    std::size_t rowOrdinal = 0;
};

[[nodiscard]] bool outranks(const ValueOrigin& candidate, const ValueOrigin& current)
{
    if (candidate.sourceRank != current.sourceRank) {
        return candidate.sourceRank > current.sourceRank;
    }
    return candidate.rowOrdinal < current.rowOrdinal;
}

// Origin of every value field the merge enriches, per survivor. A field keeps
// the origin of the source and row that actually supplied its value, so a
// survivor that inherited a value never promotes it to its own, higher source
// rank. The absent markers of the value model decide presence, so an origin is
// present exactly when the survivor carries a value for that field and a valid
// zero is a value like any other. Unions without precedence, such as
// identifiers and aliases, carry no origin.
struct FieldOrigins final {
    std::optional<ValueOrigin> displayName;
    std::optional<ValueOrigin> fixedEquatorial;
    std::optional<ValueOrigin> properMotionRightAscension;
    std::optional<ValueOrigin> properMotionDeclination;
    std::optional<ValueOrigin> stellarParallax;
    std::optional<ValueOrigin> radialVelocity;
    std::optional<ValueOrigin> astrometryValidityRange;
    std::optional<ValueOrigin> deepSkyKind;
    std::optional<ValueOrigin> majorAxis;
    std::optional<ValueOrigin> minorAxis;
    std::optional<ValueOrigin> positionAngle;
};

// Origins of the values a freshly parsed record supplies: every present field
// belongs to the source that owns the record, at that record's row order. The
// absent markers of the value model decide presence: an empty display name is
// absent, an empty deep-sky kind is absent, and every optional value counts as
// soon as it has a value.
[[nodiscard]] FieldOrigins
fieldOriginsOf(const BaseCelestialBody& body, const std::size_t sourceRank, const std::size_t rowOrdinal)
{
    const ValueOrigin origin{sourceRank, rowOrdinal};
    FieldOrigins origins;
    if (!body.displayName.empty()) {
        origins.displayName = origin;
    }
    if (body.fixedEquatorialValue().has_value()) {
        origins.fixedEquatorial = origin;
    }

    const CatalogStarAstrometry* astrometry = body.catalogStarAstrometry();
    if (astrometry != nullptr) {
        if (astrometry->properMotionRightAscensionMasPerYear.has_value()) {
            origins.properMotionRightAscension = origin;
        }
        if (astrometry->properMotionDeclinationMasPerYear.has_value()) {
            origins.properMotionDeclination = origin;
        }
        if (astrometry->stellarParallaxMas.has_value()) {
            origins.stellarParallax = origin;
        }
        if (astrometry->radialVelocityKmPerSecond.has_value()) {
            origins.radialVelocity = origin;
        }
        if (astrometry->validityRange.has_value()) {
            origins.astrometryValidityRange = origin;
        }
    }

    const DeepSkyObjectInfo* deepSky = body.deepSkyObjectInfo();
    if (deepSky != nullptr) {
        if (deepSky->kind != DeepSkyObjectInfo::Kind::Unknown) {
            origins.deepSkyKind = origin;
        }
        if (deepSky->majorAxisArcmin.has_value()) {
            origins.majorAxis = origin;
        }
        if (deepSky->minorAxisArcmin.has_value()) {
            origins.minorAxis = origin;
        }
        if (deepSky->positionAngleDeg.has_value()) {
            origins.positionAngle = origin;
        }
    }
    return origins;
}

// Accumulates deduplicated bodies of one or more sources while keeping logical
// positions stable so an identity index can address each survivor by value.
// A position can be vacated when a later replacing source wins; vacated
// positions are skipped during final assembly. The field origins of a survivor
// are kept beside its body, aligned by position.
struct MergeAccumulator final {
    std::vector<OwnGalaxyCelestialBody> ownGalaxyBodies;
    std::vector<DistantCelestialBody> distantBodies;
    std::vector<std::string> sourceIds;
    std::vector<std::vector<std::string>> contributorSourceIds;
    std::vector<FieldOrigins> fieldOrigins;
    std::vector<bool> isDistant;
    std::vector<std::size_t> domainIndex;
    std::vector<bool> active;

    // Appends a body owned by `sourceId`, together with the origins of the values
    // it carries. The owning source heads the contributor list, followed by the
    // prior contributors in the descending source precedence order the caller
    // established. Prior ids are deduplicated, so a source absorbed through
    // several paths is listed once.
    [[nodiscard]] std::size_t append(
        const BaseCelestialBody& body,
        std::string sourceId,
        FieldOrigins origins,
        std::vector<std::string> priorContributors = {}
    )
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
        fieldOrigins.push_back(std::move(origins));
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

void logFixedCoordinateConflict(const BaseCelestialBody& kept, const BaseCelestialBody& conflicting)
{
    const QString message =
        QStringLiteral(
            "Catalog composition kept the fixed coordinates of %1 over conflicting fixed coordinates from %2."
        )
            .arg(bodyLabel(kept), bodyLabel(conflicting));
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

// How a merge treats a value that both the winner and the loser supply. The
// per-source dedup pass keeps the winner's value, because it processes rows in
// input order and can only merge a record into a survivor of an earlier row.
// An absorbed survivor offers its values by full origin precedence: a value
// the winner inherited from a lower-ranked source, or from a later row of an
// equal-ranked one, is replaced by the origin that actually supplied it. A
// record of the current source that resolves to a survivor this same pass
// already produced keeps inherited values and resolves a value both records
// supply from the current source by the earlier supplying row, because bridge
// absorption can reorder survivors relative to row order.
enum class ValuePrecedence {
    KeepWinnerValue,
    TakeHigherOrigin,
    TakeEarlierSameSourceRow
};

// True when a merge adopts the loser's value for a field the winner already
// supplies. KeepWinnerValue never replaces the winner's value. TakeHigherOrigin
// adopts the value whose recorded origin outranks the winner's, across sources
// and rows. TakeEarlierSameSourceRow keeps a value the winner inherited from
// another source and adopts the value supplied by the earlier row when both
// values come from the same source, which every source rank identifies
// uniquely.
[[nodiscard]] bool
loserValueWins(const ValueOrigin& loserOrigin, const ValueOrigin& winnerOrigin, const ValuePrecedence precedence)
{
    switch (precedence) {
    case ValuePrecedence::KeepWinnerValue:
        return false;
    case ValuePrecedence::TakeHigherOrigin:
        return outranks(loserOrigin, winnerOrigin);
    case ValuePrecedence::TakeEarlierSameSourceRow:
        return loserOrigin.sourceRank == winnerOrigin.sourceRank && outranks(loserOrigin, winnerOrigin);
    }
    return false;
}

// Adopts the loser's value when the winner carries none, or when the active
// precedence mode lets the loser's recorded origin win.
template <typename T>
void takeHigherPrecedence(
    std::optional<T>& winnerValue,
    std::optional<ValueOrigin>& winnerOrigin,
    const std::optional<T>& loserValue,
    const std::optional<ValueOrigin> loserOrigin,
    const ValuePrecedence precedence
)
{
    if (!loserValue.has_value()) {
        return;
    }
    Q_ASSERT(!winnerValue.has_value() || winnerOrigin.has_value());
    Q_ASSERT(!loserValue.has_value() || loserOrigin.has_value());
    if (winnerValue.has_value() && !loserValueWins(*loserOrigin, *winnerOrigin, precedence)) {
        return;
    }

    winnerValue = loserValue;
    winnerOrigin = loserOrigin;
}

void takeHigherPrecedence(
    std::string& winnerValue,
    std::optional<ValueOrigin>& winnerOrigin,
    const std::string& loserValue,
    const std::optional<ValueOrigin> loserOrigin,
    const ValuePrecedence precedence
)
{
    if (loserValue.empty()) {
        return;
    }
    Q_ASSERT(winnerValue.empty() || winnerOrigin.has_value());
    Q_ASSERT(loserValue.empty() || loserOrigin.has_value());
    if (!winnerValue.empty() && !loserValueWins(*loserOrigin, *winnerOrigin, precedence)) {
        return;
    }

    winnerValue = loserValue;
    winnerOrigin = loserOrigin;
}

// The deep-sky kind has no optional wrapper: an empty kind is its absent
// marker, exactly like an empty display name.
void takeHigherPrecedence(
    DeepSkyObjectInfo::Kind& winnerValue,
    std::optional<ValueOrigin>& winnerOrigin,
    const DeepSkyObjectInfo::Kind loserValue,
    const std::optional<ValueOrigin> loserOrigin,
    const ValuePrecedence precedence
)
{
    if (loserValue == DeepSkyObjectInfo::Kind::Unknown) {
        return;
    }
    Q_ASSERT(winnerValue == DeepSkyObjectInfo::Kind::Unknown || winnerOrigin.has_value());
    Q_ASSERT(loserValue == DeepSkyObjectInfo::Kind::Unknown || loserOrigin.has_value());
    if (winnerValue != DeepSkyObjectInfo::Kind::Unknown && !loserValueWins(*loserOrigin, *winnerOrigin, precedence)) {
        return;
    }

    winnerValue = loserValue;
    winnerOrigin = loserOrigin;
}

// Merges the losing fixed position into the winning coordinate model. The
// position supplied by the highest-precedence source wins, so a losing position
// replaces a winning position only when the winning position was itself
// inherited from a lower-precedence source. A losing position never enters a
// model whose astrometry already anchors the object at a different direction.
// A fixed position carries no reference epoch, so the comparison against a
// reference position has no epoch conversion to apply.
void mergeFixedEquatorialInPlace(
    OwnGalaxyCelestialBody& winner,
    FieldOrigins& winnerOrigins,
    const BaseCelestialBody& loser,
    const FieldOrigins& loserOrigins,
    const ValuePrecedence precedence
)
{
    const skygate::core::EquatorialCoordinate* loserFixed = loser.fixedEquatorialCoordinate();
    if (loserFixed == nullptr) {
        return;
    }

    const bool winnerHasFixed = winner.fixedEquatorial.has_value();
    const bool loserOutranks =
        !winnerHasFixed
        || (winnerOrigins.fixedEquatorial.has_value() && loserOrigins.fixedEquatorial.has_value()
            && loserValueWins(*loserOrigins.fixedEquatorial, *winnerOrigins.fixedEquatorial, precedence));
    if (!loserOutranks) {
        if (winnerHasFixed && coordinatesConflict(*winner.fixedEquatorial, *loserFixed)) {
            logFixedCoordinateConflict(winner, loser);
        }
        return;
    }

    if (winner.starAstrometry.has_value()
        && !CatalogCoordinateModel::sameDirection(winner.starAstrometry->referenceEquatorial, *loserFixed)) {
        logRejectedFixedCoordinates(winner, loser);
        return;
    }

    if (winnerHasFixed && coordinatesConflict(*winner.fixedEquatorial, *loserFixed)) {
        logFixedCoordinateConflict(loser, winner);
    }
    winner.fixedEquatorial = *loserFixed;
    winnerOrigins.fixedEquatorial = loserOrigins.fixedEquatorial;
}

// Merges the losing astrometry into the winning coordinate model. The
// winner's reference position and reference epoch are authoritative. A losing
// astrometry only enters the record when it agrees with the surviving model:
// directly against a fixed position, or against the winning reference
// position after the losing proper motion accounts for the reference-epoch
// difference. Compatible losing astrometry fills the fields the winner is
// missing and replaces a field whose winning value was inherited from a
// lower-precedence source; incompatible astrometry is rejected and diagnosed
// instead of producing a record with two contradictory coordinate
// descriptions.
void mergeStarAstrometryInPlace(
    OwnGalaxyCelestialBody& winner,
    FieldOrigins& winnerOrigins,
    const BaseCelestialBody& loser,
    const FieldOrigins& loserOrigins,
    const ValuePrecedence precedence
)
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
        winnerOrigins.properMotionRightAscension = loserOrigins.properMotionRightAscension;
        winnerOrigins.properMotionDeclination = loserOrigins.properMotionDeclination;
        winnerOrigins.stellarParallax = loserOrigins.stellarParallax;
        winnerOrigins.radialVelocity = loserOrigins.radialVelocity;
        winnerOrigins.astrometryValidityRange = loserOrigins.astrometryValidityRange;
        return;
    }

    if (!CatalogCoordinateModel::sameAstrometry(*winner.starAstrometry, *loserAstrometry)) {
        logRejectedAstrometry(winner, loser, QStringLiteral("reference coordinates"));
        return;
    }

    CatalogStarAstrometry& merged = *winner.starAstrometry;
    takeHigherPrecedence(
        merged.properMotionRightAscensionMasPerYear,
        winnerOrigins.properMotionRightAscension,
        loserAstrometry->properMotionRightAscensionMasPerYear,
        loserOrigins.properMotionRightAscension,
        precedence
    );
    takeHigherPrecedence(
        merged.properMotionDeclinationMasPerYear,
        winnerOrigins.properMotionDeclination,
        loserAstrometry->properMotionDeclinationMasPerYear,
        loserOrigins.properMotionDeclination,
        precedence
    );
    takeHigherPrecedence(
        merged.stellarParallaxMas,
        winnerOrigins.stellarParallax,
        loserAstrometry->stellarParallaxMas,
        loserOrigins.stellarParallax,
        precedence
    );
    takeHigherPrecedence(
        merged.radialVelocityKmPerSecond,
        winnerOrigins.radialVelocity,
        loserAstrometry->radialVelocityKmPerSecond,
        loserOrigins.radialVelocity,
        precedence
    );
    takeHigherPrecedence(
        merged.validityRange,
        winnerOrigins.astrometryValidityRange,
        loserAstrometry->validityRange,
        loserOrigins.astrometryValidityRange,
        precedence
    );
}

void mergeOwnGalaxyInPlace(
    OwnGalaxyCelestialBody& winner,
    FieldOrigins& winnerOrigins,
    const BaseCelestialBody& loser,
    const FieldOrigins& loserOrigins,
    const ValuePrecedence precedence
)
{
    mergeIdentityInto(winner.identity, loser.identity);
    takeHigherPrecedence(
        winner.displayName, winnerOrigins.displayName, loser.displayName, loserOrigins.displayName, precedence
    );

    mergeFixedEquatorialInPlace(winner, winnerOrigins, loser, loserOrigins, precedence);
    mergeStarAstrometryInPlace(winner, winnerOrigins, loser, loserOrigins, precedence);
}

void mergeDistantInPlace(
    DistantCelestialBody& winner,
    FieldOrigins& winnerOrigins,
    const BaseCelestialBody& loser,
    const FieldOrigins& loserOrigins,
    const ValuePrecedence precedence
)
{
    mergeIdentityInto(winner.identity, loser.identity);
    takeHigherPrecedence(
        winner.displayName, winnerOrigins.displayName, loser.displayName, loserOrigins.displayName, precedence
    );

    const skygate::core::EquatorialCoordinate* loserFixed = loser.fixedEquatorialCoordinate();
    if (loserFixed != nullptr) {
        const bool winnerHasFixed = winner.fixedEquatorial.has_value();
        const bool loserOutranks =
            !winnerHasFixed
            || (winnerOrigins.fixedEquatorial.has_value() && loserOrigins.fixedEquatorial.has_value()
                && loserValueWins(*loserOrigins.fixedEquatorial, *winnerOrigins.fixedEquatorial, precedence));
        if (loserOutranks) {
            if (winnerHasFixed && coordinatesConflict(*winner.fixedEquatorial, *loserFixed)) {
                logFixedCoordinateConflict(loser, winner);
            }
            winner.fixedEquatorial = *loserFixed;
            winnerOrigins.fixedEquatorial = loserOrigins.fixedEquatorial;
        } else if (winnerHasFixed && coordinatesConflict(*winner.fixedEquatorial, *loserFixed)) {
            logFixedCoordinateConflict(winner, loser);
        }
    }

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
    takeHigherPrecedence(
        merged.kind, winnerOrigins.deepSkyKind, loserDeepSky->kind, loserOrigins.deepSkyKind, precedence
    );
    takeHigherPrecedence(
        merged.majorAxisArcmin,
        winnerOrigins.majorAxis,
        loserDeepSky->majorAxisArcmin,
        loserOrigins.majorAxis,
        precedence
    );
    takeHigherPrecedence(
        merged.minorAxisArcmin,
        winnerOrigins.minorAxis,
        loserDeepSky->minorAxisArcmin,
        loserOrigins.minorAxis,
        precedence
    );
    takeHigherPrecedence(
        merged.positionAngleDeg,
        winnerOrigins.positionAngle,
        loserDeepSky->positionAngleDeg,
        loserOrigins.positionAngle,
        precedence
    );
    mergeAliasesInto(merged.aliases, loserDeepSky->aliases);
}

void mergeSurvivorInPlace(
    BaseCelestialBody& winner,
    FieldOrigins& winnerOrigins,
    const BaseCelestialBody& loser,
    const FieldOrigins& loserOrigins,
    const ValuePrecedence precedence
)
{
    retainCanonicalEquivalences(winner.identity, winner.id, loser);
    if (winner.kind == BaseCelestialBody::Kind::DeepSkyObject) {
        mergeDistantInPlace(static_cast<DistantCelestialBody&>(winner), winnerOrigins, loser, loserOrigins, precedence);
        return;
    }
    mergeOwnGalaxyInPlace(static_cast<OwnGalaxyCelestialBody&>(winner), winnerOrigins, loser, loserOrigins, precedence);
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

// Earliest row that supplied any part of a survivor's coordinate model. Model
// fields of absorbed survivors of one source merge in that row order, so the
// first row that supplied a model also decides which coherent model survives;
// a survivor without a model orders after every model a row supplied.
[[nodiscard]] std::size_t earliestModelRow(const FieldOrigins& origins)
{
    std::size_t first = std::numeric_limits<std::size_t>::max();
    const std::optional<ValueOrigin>* const modelFields[] = {
        &origins.fixedEquatorial,
        &origins.properMotionRightAscension,
        &origins.properMotionDeclination,
        &origins.stellarParallax,
        &origins.radialVelocity,
        &origins.astrometryValidityRange,
    };
    for (const std::optional<ValueOrigin>* field : modelFields) {
        if (field->has_value()) {
            first = std::min(first, (*field)->rowOrdinal);
        }
    }
    return first;
}

// Merges every absorbed survivor into `winner` in configured source precedence
// order, highest precedence first, and within one source in the order the rows
// supplied their coordinate models. Each value is filled or replaced by the
// absorbed survivor whose value origin outranks the origin the winner carries:
// the source that actually supplied the value with the highest rank, or, when
// both values belong to one source, the earlier row of that source, because the
// first row of a source is authoritative and its later rows only fill it. So a
// value a lower-precedence source supplied through an intermediate survivor is
// never promoted to that survivor's rank, and a bridge across survivors of one
// source never outranks the rows that supplied their metadata. Each merge
// diagnoses the coordinate or metadata conflicts the winner overrides.
void absorbSurvivors(
    BaseCelestialBody& winner,
    FieldOrigins& winnerOrigins,
    const MergeAccumulator& accumulator,
    std::vector<std::size_t> positions,
    const SourcePrecedence& sourcePrecedence
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
            return earliestModelRow(accumulator.fieldOrigins[lhs]) < earliestModelRow(accumulator.fieldOrigins[rhs]);
        }
    );

    for (const std::size_t position : positions) {
        mergeSurvivorInPlace(
            winner,
            winnerOrigins,
            accumulator.at(position),
            accumulator.fieldOrigins[position],
            ValuePrecedence::TakeHigherOrigin
        );
    }
}

// Appends `incoming` as the survivor of every absorbed position. The incoming
// record supplies the public canonical id because it is the later record that
// establishes the shared authoritative identity or replaces an earlier
// source's survivor; its values keep their own origins and only supply fields
// the absorbed survivors do not. Absorbed positions are vacated and removed
// from the index before the winner is registered, so later rows resolve to the
// winner and never to a vacated body. Absorption retains each absorbed body's
// canonical id and canonical equivalences on the winner, so the earlier keys
// keep resolving to the survivor. A value the incoming record does not supply
// follows the origin precedence of the absorbed survivors that do, and the
// contributor id order follows configured source precedence rather than
// accumulator position order.
std::size_t absorbSurvivorsAndAppend(
    MergeAccumulator& accumulator,
    CatalogIdentityIndex& index,
    const BaseCelestialBody& incoming,
    FieldOrigins incomingOrigins,
    const std::vector<std::size_t>& absorbedPositions,
    std::string sourceId,
    const SourcePrecedence& sourcePrecedence
)
{
    std::vector<std::string> priorContributors = collectContributors(accumulator, absorbedPositions, sourcePrecedence);
    FieldOrigins winnerOrigins = std::move(incomingOrigins);
    std::size_t position = 0;
    if (incoming.kind == BaseCelestialBody::Kind::DeepSkyObject) {
        DistantCelestialBody winner = CelestialBodyCatalog::copyDistantBody(incoming);
        absorbSurvivors(winner, winnerOrigins, accumulator, absorbedPositions, sourcePrecedence);
        for (const std::size_t absorbed : absorbedPositions) {
            accumulator.vacate(absorbed);
            index.remove(absorbed);
        }
        position =
            accumulator.append(winner, std::move(sourceId), std::move(winnerOrigins), std::move(priorContributors));
    } else {
        OwnGalaxyCelestialBody winner = CelestialBodyCatalog::copyOwnGalaxyBody(incoming);
        absorbSurvivors(winner, winnerOrigins, accumulator, absorbedPositions, sourcePrecedence);
        for (const std::size_t absorbed : absorbedPositions) {
            accumulator.vacate(absorbed);
            index.remove(absorbed);
        }
        position =
            accumulator.append(winner, std::move(sourceId), std::move(winnerOrigins), std::move(priorContributors));
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
// sources never match. `rowOrdinal` is the position of the record in the
// source, so every value it supplies keeps its true row origin.
std::size_t appendDeduped(
    MergeAccumulator& accumulator,
    CatalogIdentityIndex& index,
    const BaseCelestialBody& incoming,
    const std::string_view sourceId,
    const SourcePrecedence& sourcePrecedence,
    const std::size_t rowOrdinal
)
{
    const SourceScopedBody scoped{incoming, sourceId};
    const BaseCelestialBody& body = scoped.body();
    const std::size_t sourceRank = sourcePrecedence.rankOf(sourceId);
    const MatchDecision decision = evaluateMatch(accumulator, index, body);
    if (decision.action == MatchDecision::Action::Merge) {
        mergeSurvivorInPlace(
            accumulator.at(decision.matchIndex),
            accumulator.fieldOrigins[decision.matchIndex],
            body,
            fieldOriginsOf(body, sourceRank, rowOrdinal),
            ValuePrecedence::KeepWinnerValue
        );
        // Registers the merged record's keys as well, so a later record with
        // the same record key resolves to this survivor.
        index.add(body, decision.matchIndex);
        return decision.matchIndex;
    }

    if (decision.action == MatchDecision::Action::Bridge) {
        return absorbSurvivorsAndAppend(
            accumulator,
            index,
            body,
            fieldOriginsOf(body, sourceRank, rowOrdinal),
            decision.bridgeCandidates,
            std::string(sourceId),
            sourcePrecedence
        );
    }

    const std::size_t position =
        accumulator.append(body, std::string(sourceId), fieldOriginsOf(body, sourceRank, rowOrdinal));
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

// Gap-fill policies never replace an existing survivor: a body whose shared
// identity decision resolves it to an existing equivalent is preserved
// instead of appended. AugmentCore and DeepSkyFallback differ in which bodies
// they select, not in precedence.
bool isGapFillPolicy(const CatalogCompositionPolicy policy)
{
    return policy == CatalogCompositionPolicy::AugmentCore || policy == CatalogCompositionPolicy::DeepSkyFallback;
}

// Applies the shared identity decision to one body of a gap-fill source and
// appends it only when its identity is not already established. A proven
// equivalence keeps the earlier survivor untouched; a contradicted or
// ambiguous weak alias match appends the body as an independent object with
// the normal keep-distinct diagnostics. Only replacement-versus-preservation
// is policy-specific, never the identity decision itself.
void appendGapFillBody(
    MergeAccumulator& accumulator,
    CatalogIdentityIndex& index,
    const BaseCelestialBody& body,
    const std::string_view sourceId,
    const SourcePrecedence& sourcePrecedence,
    const std::size_t rowOrdinal
)
{
    const MatchDecision decision = evaluateMatch(accumulator, index, body);
    if (decision.action != MatchDecision::Action::Append) {
        return;
    }

    const std::size_t position = accumulator.append(
        body, std::string(sourceId), fieldOriginsOf(body, sourcePrecedence.rankOf(sourceId), rowOrdinal)
    );
    index.add(accumulator.at(position), position);
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
                appendGapFillBody(
                    accumulator, activeIndex, scoped.body(), source.sourceId, sourcePrecedence, rowOrdinal++
                );
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
                    mergeSurvivorInPlace(
                        accumulator.at(decision.matchIndex),
                        accumulator.fieldOrigins[decision.matchIndex],
                        body,
                        sourceBodies.fieldOrigins[position],
                        ValuePrecedence::TakeEarlierSameSourceRow
                    );
                    activeIndex.add(body, decision.matchIndex);
                    continue;
                }

                // A later source wins over an earlier source's survivor and
                // absorbs its non-conflicting identity and metadata.
                static_cast<void>(absorbSurvivorsAndAppend(
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
                static_cast<void>(absorbSurvivorsAndAppend(
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
