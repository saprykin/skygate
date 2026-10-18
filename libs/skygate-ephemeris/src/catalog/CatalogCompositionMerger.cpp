#include "CatalogCompositionMerger.hpp"

#include "StringUtilities.hpp"
#include "catalog/CatalogCompositionLogging.hpp"
#include "catalog/CatalogCompositionPolicyRules.hpp"
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
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
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
//
// The coherent coordinate model has its own origins: `starAstrometry` is the
// origin of the astrometry reference-position/epoch anchor, present exactly
// when the survivor carries astrometry regardless of its optional fields, and
// `coordinateModel` is the origin of the survivor's model anchor, the
// higher-precedence origin among the present model components.
struct FieldOrigins final {
    std::optional<ValueOrigin> displayName;
    std::optional<ValueOrigin> visualMagnitude;
    std::optional<ValueOrigin> fixedEquatorial;
    std::optional<ValueOrigin> starAstrometry;
    std::optional<ValueOrigin> coordinateModel;
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
// absent, a NaN visual magnitude is absent, an empty deep-sky kind is absent,
// and every optional value counts as soon as it has a value.
[[nodiscard]] FieldOrigins
fieldOriginsOf(const BaseCelestialBody& body, const std::size_t sourceRank, const std::size_t rowOrdinal)
{
    const ValueOrigin origin{sourceRank, rowOrdinal};
    FieldOrigins origins;
    if (!body.displayName.empty()) {
        origins.displayName = origin;
    }
    if (!std::isnan(body.visualMagnitude)) {
        origins.visualMagnitude = origin;
    }
    if (body.fixedEquatorialValue().has_value()) {
        origins.fixedEquatorial = origin;
        origins.coordinateModel = origin;
    }

    const CatalogStarAstrometry* astrometry = body.catalogStarAstrometry();
    if (astrometry != nullptr) {
        origins.starAstrometry = origin;
        origins.coordinateModel = origin;
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

// Origin of the survivor's coherent coordinate model anchor: the
// higher-precedence origin among the model components the survivor carries, so
// replacing or clearing a component never leaves a stale anchor behind.
void refreshCoordinateModelOrigin(FieldOrigins& origins)
{
    std::optional<ValueOrigin> model;
    if (origins.fixedEquatorial.has_value()) {
        model = origins.fixedEquatorial;
    }
    if (origins.starAstrometry.has_value() && (!model.has_value() || outranks(*origins.starAstrometry, *model))) {
        model = origins.starAstrometry;
    }
    origins.coordinateModel = std::move(model);
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
            StringUtilities::appendUnique(contributors, std::move(prior));
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

// Retains the union of identifiers from both records, including several
// values in the same namespace (a single object can carry multiple NGC or IC
// designations).
void mergeExternalIdentifiersInto(CatalogObjectIdentity& winner, const CatalogObjectIdentity& loser)
{
    for (const CatalogIdentifier& identifier : loser.externalIdentifiers) {
        CatalogIdentifier::appendUnique(winner.externalIdentifiers, identifier);
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

// True when the loser's coherent coordinate model outranks the winner's and
// therefore replaces it whole. The loser needs a model component, and its
// anchor origin must win under the active precedence; a winner without a model
// always adopts the loser's.
[[nodiscard]] bool loserCoordinateModelWins(
    const FieldOrigins& loserOrigins, const FieldOrigins& winnerOrigins, const ValuePrecedence precedence
)
{
    if (!loserOrigins.coordinateModel.has_value()) {
        return false;
    }
    if (!winnerOrigins.coordinateModel.has_value()) {
        return true;
    }
    return loserValueWins(*loserOrigins.coordinateModel, *winnerOrigins.coordinateModel, precedence);
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

// The visual magnitude has no optional wrapper: NaN is its absent marker,
// exactly like an empty display name, and a valid zero is a value like any
// other.
void takeHigherPrecedence(
    double& winnerValue,
    std::optional<ValueOrigin>& winnerOrigin,
    const double loserValue,
    const std::optional<ValueOrigin> loserOrigin,
    const ValuePrecedence precedence
)
{
    if (std::isnan(loserValue)) {
        return;
    }
    Q_ASSERT(std::isnan(winnerValue) || winnerOrigin.has_value());
    Q_ASSERT(std::isnan(loserValue) || loserOrigin.has_value());
    if (!std::isnan(winnerValue) && !loserValueWins(*loserOrigin, *winnerOrigin, precedence)) {
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

// How one losing astrometry entered the winning coordinate model, so each
// caller can diagnose a rejection with the labels of the records it merges.
enum class AstrometryMergeOutcome {
    Adopted,
    MergedFields,
    RejectedAgainstFixed,
    RejectedAgainstReference
};

// Fills the winner's coordinate model from one losing astrometry. The losing
// astrometry only enters the model when it agrees with the surviving direction:
// directly against a fixed position, or against the winning reference position
// after the losing proper motion accounts for the reference-epoch difference.
// Compatible losing astrometry replaces a missing astrometry or fills and
// replaces its single fields under origin precedence; incompatible astrometry
// is dropped so the survivor never combines two contradictory coordinate
// descriptions.
[[nodiscard]] AstrometryMergeOutcome mergeLosingAstrometry(
    OwnGalaxyCelestialBody& winner,
    FieldOrigins& winnerOrigins,
    const CatalogStarAstrometry& loserAstrometry,
    const FieldOrigins& loserOrigins,
    const ValuePrecedence precedence
)
{
    if (!winner.starAstrometry.has_value()) {
        if (winner.fixedEquatorial.has_value()
            && !CatalogCoordinateModel::sameDirection(*winner.fixedEquatorial, loserAstrometry.referenceEquatorial)) {
            return AstrometryMergeOutcome::RejectedAgainstFixed;
        }

        winner.starAstrometry = loserAstrometry;
        winnerOrigins.starAstrometry = loserOrigins.starAstrometry;
        winnerOrigins.properMotionRightAscension = loserOrigins.properMotionRightAscension;
        winnerOrigins.properMotionDeclination = loserOrigins.properMotionDeclination;
        winnerOrigins.stellarParallax = loserOrigins.stellarParallax;
        winnerOrigins.radialVelocity = loserOrigins.radialVelocity;
        winnerOrigins.astrometryValidityRange = loserOrigins.astrometryValidityRange;
        refreshCoordinateModelOrigin(winnerOrigins);
        return AstrometryMergeOutcome::Adopted;
    }

    if (!CatalogCoordinateModel::sameAstrometry(*winner.starAstrometry, loserAstrometry)) {
        return AstrometryMergeOutcome::RejectedAgainstReference;
    }

    CatalogStarAstrometry& merged = *winner.starAstrometry;
    takeHigherPrecedence(
        merged.properMotionRightAscensionMasPerYear,
        winnerOrigins.properMotionRightAscension,
        loserAstrometry.properMotionRightAscensionMasPerYear,
        loserOrigins.properMotionRightAscension,
        precedence
    );
    takeHigherPrecedence(
        merged.properMotionDeclinationMasPerYear,
        winnerOrigins.properMotionDeclination,
        loserAstrometry.properMotionDeclinationMasPerYear,
        loserOrigins.properMotionDeclination,
        precedence
    );
    takeHigherPrecedence(
        merged.stellarParallaxMas,
        winnerOrigins.stellarParallax,
        loserAstrometry.stellarParallaxMas,
        loserOrigins.stellarParallax,
        precedence
    );
    takeHigherPrecedence(
        merged.radialVelocityKmPerSecond,
        winnerOrigins.radialVelocity,
        loserAstrometry.radialVelocityKmPerSecond,
        loserOrigins.radialVelocity,
        precedence
    );
    takeHigherPrecedence(
        merged.validityRange,
        winnerOrigins.astrometryValidityRange,
        loserAstrometry.validityRange,
        loserOrigins.astrometryValidityRange,
        precedence
    );
    return AstrometryMergeOutcome::MergedFields;
}

// Replaces the winner's whole coordinate model with the loser's when the
// loser's model anchor outranks the winner's: the higher-precedence model is
// selected before any compatibility check, so the model the winner happened to
// carry first never rejects the model its origin outranks. The selected model
// carries its own fixed position, reference position, and reference epoch; the
// replaced model's compatible astrometry still enriches it field by field,
// while a replaced position or astrometry that contradicts the selected model
// is diagnosed instead of being combined with it. A compatible replaced fixed
// position is dropped with the rest of the replaced model. A loser whose own
// fixed position and astrometry contradict each other is adopted as its fixed
// position alone, because carrying its astrometry onto the winner would
// publish the same contradiction on the survivor; the astrometry merge then
// rejects and diagnoses the discarded astrometry.
void replaceCoordinateModelInPlace(
    OwnGalaxyCelestialBody& winner,
    FieldOrigins& winnerOrigins,
    const BaseCelestialBody& loser,
    const FieldOrigins& loserOrigins,
    const ValuePrecedence precedence
)
{
    const std::optional<skygate::core::EquatorialCoordinate> replacedFixed = winner.fixedEquatorial;
    const std::optional<CatalogStarAstrometry> replacedAstrometry = winner.starAstrometry;
    const FieldOrigins replacedOrigins = winnerOrigins;
    const std::optional<skygate::core::EquatorialCoordinate> loserFixed = loser.fixedEquatorialValue();
    const CatalogStarAstrometry* loserAstrometry = loser.catalogStarAstrometry();
    const bool replacedFixedConflicts = replacedFixed.has_value() && loserFixed.has_value()
                                        && CatalogCoordinateModel::positionsDiffer(*replacedFixed, *loserFixed);
    const bool loserAstrometryContradictsFixed =
        loserFixed.has_value() && loserAstrometry != nullptr
        && !CatalogCoordinateModel::sameDirection(loserAstrometry->referenceEquatorial, *loserFixed);

    winner.fixedEquatorial = loserFixed;
    winner.starAstrometry = loser.starAstrometryValue();
    winnerOrigins.fixedEquatorial = loserOrigins.fixedEquatorial;
    winnerOrigins.starAstrometry = loserOrigins.starAstrometry;
    winnerOrigins.properMotionRightAscension = loserOrigins.properMotionRightAscension;
    winnerOrigins.properMotionDeclination = loserOrigins.properMotionDeclination;
    winnerOrigins.stellarParallax = loserOrigins.stellarParallax;
    winnerOrigins.radialVelocity = loserOrigins.radialVelocity;
    winnerOrigins.astrometryValidityRange = loserOrigins.astrometryValidityRange;

    if (loserAstrometryContradictsFixed) {
        winner.starAstrometry = std::nullopt;
        winnerOrigins.starAstrometry = std::nullopt;
        winnerOrigins.properMotionRightAscension = std::nullopt;
        winnerOrigins.properMotionDeclination = std::nullopt;
        winnerOrigins.stellarParallax = std::nullopt;
        winnerOrigins.radialVelocity = std::nullopt;
        winnerOrigins.astrometryValidityRange = std::nullopt;
    }

    if (replacedFixedConflicts) {
        logFixedCoordinateConflict(loser, winner);
    } else if (
        replacedFixed.has_value() && !winner.fixedEquatorial.has_value()
        && (!winner.starAstrometry.has_value()
            || !CatalogCoordinateModel::sameDirection(winner.starAstrometry->referenceEquatorial, *replacedFixed))
    ) {
        logRejectedFixedCoordinates(loser, winner);
    }

    if (replacedAstrometry.has_value()) {
        switch (mergeLosingAstrometry(winner, winnerOrigins, *replacedAstrometry, replacedOrigins, precedence)) {
        case AstrometryMergeOutcome::RejectedAgainstFixed:
            logRejectedAstrometry(loser, winner, QStringLiteral("fixed coordinates"));
            break;
        case AstrometryMergeOutcome::RejectedAgainstReference:
            logRejectedAstrometry(loser, winner, QStringLiteral("reference coordinates"));
            break;
        case AstrometryMergeOutcome::Adopted:
        case AstrometryMergeOutcome::MergedFields:
            break;
        }
    }

    refreshCoordinateModelOrigin(winnerOrigins);
}

// Merges the losing fixed position into the winning fixed coordinate under
// origin precedence. A losing position replaces a winning position only when
// the winning position was itself inherited from a lower-precedence source,
// and it never enters a model whose astrometry already anchors the object at a
// different direction. A fixed position carries no reference epoch, so the
// comparison against a reference position has no epoch conversion to apply.
// A winner without astrometry reports no reference direction, so the same
// helper serves both body domains.
template <typename WinnerBody>
void mergeLosingFixedPosition(
    WinnerBody& winner,
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
        if (winnerHasFixed && CatalogCoordinateModel::positionsDiffer(*winner.fixedEquatorial, *loserFixed)) {
            logFixedCoordinateConflict(winner, loser);
        }
        return;
    }

    const std::optional<CatalogStarAstrometry>& winnerAstrometry = winner.starAstrometryValue();
    if (winnerAstrometry.has_value()
        && !CatalogCoordinateModel::sameDirection(winnerAstrometry->referenceEquatorial, *loserFixed)) {
        logRejectedFixedCoordinates(winner, loser);
        return;
    }

    if (winnerHasFixed && CatalogCoordinateModel::positionsDiffer(*winner.fixedEquatorial, *loserFixed)) {
        logFixedCoordinateConflict(loser, winner);
    }
    winner.fixedEquatorial = *loserFixed;
    winnerOrigins.fixedEquatorial = loserOrigins.fixedEquatorial;
    refreshCoordinateModelOrigin(winnerOrigins);
}

// Merges the losing fixed position into the winning coordinate model. A loser
// whose coherent model anchor outranks the winner's replaces the whole model
// before this per-component merge, so a position that disagrees with the model
// whose origin loses never rejects the earlier model.
void mergeFixedEquatorialInPlace(
    OwnGalaxyCelestialBody& winner,
    FieldOrigins& winnerOrigins,
    const BaseCelestialBody& loser,
    const FieldOrigins& loserOrigins,
    const ValuePrecedence precedence
)
{
    if (loserCoordinateModelWins(loserOrigins, winnerOrigins, precedence)) {
        replaceCoordinateModelInPlace(winner, winnerOrigins, loser, loserOrigins, precedence);
        return;
    }

    mergeLosingFixedPosition(winner, winnerOrigins, loser, loserOrigins, precedence);
}

// Merges the losing astrometry into the winning coordinate model. A loser
// whose coherent model anchor outranks the winner's replaces the whole model
// before this per-component merge, exactly like a losing fixed position.
// Otherwise the winner's reference position and reference epoch are
// authoritative. A losing astrometry only enters the record when it agrees
// with the surviving model:
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
    if (loserCoordinateModelWins(loserOrigins, winnerOrigins, precedence)) {
        replaceCoordinateModelInPlace(winner, winnerOrigins, loser, loserOrigins, precedence);
        return;
    }

    const CatalogStarAstrometry* loserAstrometry = loser.catalogStarAstrometry();
    if (loserAstrometry == nullptr) {
        return;
    }

    switch (mergeLosingAstrometry(winner, winnerOrigins, *loserAstrometry, loserOrigins, precedence)) {
    case AstrometryMergeOutcome::RejectedAgainstFixed:
        logRejectedAstrometry(winner, loser, QStringLiteral("fixed coordinates"));
        break;
    case AstrometryMergeOutcome::RejectedAgainstReference:
        logRejectedAstrometry(winner, loser, QStringLiteral("reference coordinates"));
        break;
    case AstrometryMergeOutcome::Adopted:
    case AstrometryMergeOutcome::MergedFields:
        break;
    }
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
    takeHigherPrecedence(
        winner.visualMagnitude,
        winnerOrigins.visualMagnitude,
        loser.visualMagnitude,
        loserOrigins.visualMagnitude,
        precedence
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
    takeHigherPrecedence(
        winner.visualMagnitude,
        winnerOrigins.visualMagnitude,
        loser.visualMagnitude,
        loserOrigins.visualMagnitude,
        precedence
    );

    mergeLosingFixedPosition(winner, winnerOrigins, loser, loserOrigins, precedence);

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
            const std::optional<ValueOrigin>& lhsModel = accumulator.fieldOrigins[lhs].coordinateModel;
            const std::optional<ValueOrigin>& rhsModel = accumulator.fieldOrigins[rhs].coordinateModel;
            if (lhsModel.has_value() != rhsModel.has_value()) {
                return lhsModel.has_value();
            }
            return lhsModel.has_value() && outranks(*lhsModel, *rhsModel);
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

// Appends `winner` as the survivor of every absorbed position. The absorbed
// survivors merge into `winner` under origin precedence, and their positions
// are vacated and removed from the index before the survivor is registered, so
// later rows resolve to the new survivor and never to a vacated body.
template <typename WinnerBody>
std::size_t appendAbsorbedSurvivor(
    MergeAccumulator& accumulator,
    CatalogIdentityIndex& index,
    WinnerBody winner,
    FieldOrigins winnerOrigins,
    const std::vector<std::size_t>& absorbedPositions,
    std::string sourceId,
    std::vector<std::string> priorContributors,
    const SourcePrecedence& sourcePrecedence
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

        MergeAccumulator sourceBodies;
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
