#include "CatalogCoordinateMerge.hpp"

#include "CatalogCompositionLogging.hpp"
#include "CatalogCoordinateModel.hpp"
#include "CatalogStarAstrometry.hpp"
#include "EquatorialCoordinate.hpp"
#include "OwnGalaxyCelestialBody.hpp"

#include <QString>
#include <QtGlobal>

#include <cmath>
#include <optional>
#include <string>

namespace skygate::ephemeris {

QString CatalogCoordinateMerge::bodyLabel(const BaseCelestialBody& body)
{
    return QString::fromStdString(body.id);
}

void CatalogCoordinateMerge::logFixedCoordinateConflict(
    const BaseCelestialBody& kept, const BaseCelestialBody& conflicting
)
{
    const QString message =
        QStringLiteral(
            "Catalog composition kept the fixed coordinates of %1 over conflicting fixed coordinates from %2."
        )
            .arg(bodyLabel(kept), bodyLabel(conflicting));
    qCWarning(skygateCatalogCompositionLog).noquote() << message;
}

void CatalogCoordinateMerge::logRejectedAstrometry(
    const BaseCelestialBody& winner, const BaseCelestialBody& loser, const QString& keptModel
)
{
    const QString message =
        QStringLiteral("Catalog composition kept the %1 of %2 and rejected the incompatible astrometry of %3.")
            .arg(keptModel, bodyLabel(winner), bodyLabel(loser));
    qCWarning(skygateCatalogCompositionLog).noquote() << message;
}

void CatalogCoordinateMerge::logRejectedFixedCoordinates(
    const BaseCelestialBody& winner, const BaseCelestialBody& loser
)
{
    const QString message = QStringLiteral(
                                "Catalog composition kept the reference coordinates of %1 and rejected the "
                                "incompatible fixed coordinates of %2."
    )
                                .arg(bodyLabel(winner), bodyLabel(loser));
    qCWarning(skygateCatalogCompositionLog).noquote() << message;
}

// True when a merge adopts the loser's value for a field the winner already
// supplies. KeepWinnerValue never replaces the winner's value. TakeHigherOrigin
// adopts the value whose recorded origin outranks the winner's, across sources
// and rows. TakeEarlierSameSourceRow keeps a value the winner inherited from
// another source and adopts the value supplied by the earlier row when both
// values come from the same source, which every source rank identifies
// uniquely.
[[nodiscard]] bool CatalogCoordinateMerge::loserValueWins(
    const CatalogFieldOrigins::ValueOrigin& loserOrigin,
    const CatalogFieldOrigins::ValueOrigin& winnerOrigin,
    const ValuePrecedence precedence
)
{
    switch (precedence) {
    case ValuePrecedence::KeepWinnerValue:
        return false;
    case ValuePrecedence::TakeHigherOrigin:
        return CatalogFieldOrigins::outranks(loserOrigin, winnerOrigin);
    case ValuePrecedence::TakeEarlierSameSourceRow:
        return loserOrigin.sourceRank == winnerOrigin.sourceRank
               && CatalogFieldOrigins::outranks(loserOrigin, winnerOrigin);
    }
    return false;
}

// True when the loser's coherent coordinate model outranks the winner's and
// therefore replaces it whole. The loser needs a model component, and its
// anchor origin must win under the active precedence; a winner without a model
// always adopts the loser's.
[[nodiscard]] bool CatalogCoordinateMerge::loserCoordinateModelWins(
    const CatalogFieldOrigins& loserOrigins, const CatalogFieldOrigins& winnerOrigins, const ValuePrecedence precedence
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

void CatalogCoordinateMerge::takeHigherPrecedence(
    std::string& winnerValue,
    std::optional<CatalogFieldOrigins::ValueOrigin>& winnerOrigin,
    const std::string& loserValue,
    const std::optional<CatalogFieldOrigins::ValueOrigin> loserOrigin,
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
void CatalogCoordinateMerge::takeHigherPrecedence(
    double& winnerValue,
    std::optional<CatalogFieldOrigins::ValueOrigin>& winnerOrigin,
    const double loserValue,
    const std::optional<CatalogFieldOrigins::ValueOrigin> loserOrigin,
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
void CatalogCoordinateMerge::takeHigherPrecedence(
    DeepSkyObjectInfo::Kind& winnerValue,
    std::optional<CatalogFieldOrigins::ValueOrigin>& winnerOrigin,
    const DeepSkyObjectInfo::Kind loserValue,
    const std::optional<CatalogFieldOrigins::ValueOrigin> loserOrigin,
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

// Fills the winner's coordinate model from one losing astrometry. The losing
// astrometry only enters the model when it agrees with the surviving direction:
// directly against a fixed position, or against the winning reference position
// after the losing proper motion accounts for the reference-epoch difference.
// Compatible losing astrometry replaces a missing astrometry or fills and
// replaces its single fields under origin precedence; incompatible astrometry
// is dropped so the survivor never combines two contradictory coordinate
// descriptions.
[[nodiscard]] CatalogCoordinateMerge::AstrometryMergeOutcome CatalogCoordinateMerge::mergeLosingAstrometry(
    OwnGalaxyCelestialBody& winner,
    CatalogFieldOrigins& winnerOrigins,
    const CatalogStarAstrometry& loserAstrometry,
    const CatalogFieldOrigins& loserOrigins,
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
        CatalogFieldOrigins::refreshCoordinateModelOrigin(winnerOrigins);
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
void CatalogCoordinateMerge::replaceCoordinateModelInPlace(
    OwnGalaxyCelestialBody& winner,
    CatalogFieldOrigins& winnerOrigins,
    const BaseCelestialBody& loser,
    const CatalogFieldOrigins& loserOrigins,
    const ValuePrecedence precedence
)
{
    const std::optional<skygate::core::EquatorialCoordinate> replacedFixed = winner.fixedEquatorial;
    const std::optional<CatalogStarAstrometry> replacedAstrometry = winner.starAstrometry;
    const CatalogFieldOrigins replacedOrigins = winnerOrigins;
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

    CatalogFieldOrigins::refreshCoordinateModelOrigin(winnerOrigins);
}

void CatalogCoordinateMerge::mergeFixedEquatorialInPlace(
    OwnGalaxyCelestialBody& winner,
    CatalogFieldOrigins& winnerOrigins,
    const BaseCelestialBody& loser,
    const CatalogFieldOrigins& loserOrigins,
    const ValuePrecedence precedence
)
{
    if (loserCoordinateModelWins(loserOrigins, winnerOrigins, precedence)) {
        replaceCoordinateModelInPlace(winner, winnerOrigins, loser, loserOrigins, precedence);
        return;
    }

    mergeLosingFixedPosition(winner, winnerOrigins, loser, loserOrigins, precedence);
}

void CatalogCoordinateMerge::mergeStarAstrometryInPlace(
    OwnGalaxyCelestialBody& winner,
    CatalogFieldOrigins& winnerOrigins,
    const BaseCelestialBody& loser,
    const CatalogFieldOrigins& loserOrigins,
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

}  // namespace skygate::ephemeris
