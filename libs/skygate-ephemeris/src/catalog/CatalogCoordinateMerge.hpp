#pragma once

#include "BaseCelestialBody.hpp"
#include "OwnGalaxyCelestialBody.hpp"
#include "catalog/CatalogCoordinateModel.hpp"
#include "catalog/CatalogFieldOrigins.hpp"
#include "catalog/CatalogStarAstrometry.hpp"

#include <QString>
#include <QtGlobal>

#include <optional>
#include <string>

namespace skygate::ephemeris {

// Reconciles one losing record's coordinate model with the surviving record's.
//
// A merge keeps a single coherent coordinate model per survivor. A losing model
// whose anchor outranks the winner's replaces the winner's whole model before
// any compatibility check, compatible losing components fill or replace a field
// under origin precedence, and contradicting coordinates are rejected with a
// diagnostic rather than combined. CatalogCoordinateModel owns the coherence
// rules themselves.
class CatalogCoordinateMerge final {
public:
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

    // Adopts the loser's value when the winner carries none, or when the active
    // precedence mode lets the loser's recorded origin win. Presence follows the
    // value model's own absent marker, so these overloads differ only in which
    // value counts as absent.
    template <typename T>
    static void takeHigherPrecedence(
        std::optional<T>& winnerValue,
        std::optional<CatalogFieldOrigins::ValueOrigin>& winnerOrigin,
        const std::optional<T>& loserValue,
        const std::optional<CatalogFieldOrigins::ValueOrigin> loserOrigin,
        const ValuePrecedence precedence
    )
    {
        adoptHigherPrecedence(
            winnerValue, winnerOrigin, loserValue, loserOrigin, precedence, [](const std::optional<T>& value) {
                return !value.has_value();
            }
        );
    }

    // An empty display name is the absent marker of the text fields.
    static void takeHigherPrecedence(
        std::string& winnerValue,
        std::optional<CatalogFieldOrigins::ValueOrigin>& winnerOrigin,
        const std::string& loserValue,
        const std::optional<CatalogFieldOrigins::ValueOrigin> loserOrigin,
        const ValuePrecedence precedence
    );
    // A NaN visual magnitude is the absent marker of the numeric fields, exactly
    // like an empty display name; a valid zero is a value like any other.
    static void takeHigherPrecedence(
        double& winnerValue,
        std::optional<CatalogFieldOrigins::ValueOrigin>& winnerOrigin,
        const double loserValue,
        const std::optional<CatalogFieldOrigins::ValueOrigin> loserOrigin,
        const ValuePrecedence precedence
    );
    // An unknown kind is the absent marker of the enumerated fields.
    static void takeHigherPrecedence(
        DeepSkyObjectInfo::Kind& winnerValue,
        std::optional<CatalogFieldOrigins::ValueOrigin>& winnerOrigin,
        const DeepSkyObjectInfo::Kind loserValue,
        const std::optional<CatalogFieldOrigins::ValueOrigin> loserOrigin,
        const ValuePrecedence precedence
    );

    // Diagnostic label of a merged record.
    [[nodiscard]] static QString bodyLabel(const BaseCelestialBody& body);

    // Merges the losing fixed position into the winning fixed coordinate under
    // origin precedence. A losing position replaces a winning position only when
    // the winning position was itself inherited from a lower-precedence source,
    // and it never enters a model whose astrometry already anchors the object at a
    // different direction. A fixed position carries no reference epoch, so the
    // comparison against a reference position has no epoch conversion to apply.
    // A winner without astrometry reports no reference direction, so the same
    // helper serves both body domains.
    template <typename WinnerBody>
    static void mergeLosingFixedPosition(
        WinnerBody& winner,
        CatalogFieldOrigins& winnerOrigins,
        const BaseCelestialBody& loser,
        const CatalogFieldOrigins& loserOrigins,
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
        CatalogFieldOrigins::refreshCoordinateModelOrigin(winnerOrigins);
    }

    // Merges the losing fixed position into the winning coordinate model. A loser
    // whose coherent model anchor outranks the winner's replaces the whole model
    // before this per-component merge, so a position that disagrees with the model
    // whose origin loses never rejects the earlier model.
    static void mergeFixedEquatorialInPlace(
        OwnGalaxyCelestialBody& winner,
        CatalogFieldOrigins& winnerOrigins,
        const BaseCelestialBody& loser,
        const CatalogFieldOrigins& loserOrigins,
        const ValuePrecedence precedence
    );

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
    static void mergeStarAstrometryInPlace(
        OwnGalaxyCelestialBody& winner,
        CatalogFieldOrigins& winnerOrigins,
        const BaseCelestialBody& loser,
        const CatalogFieldOrigins& loserOrigins,
        const ValuePrecedence precedence
    );

private:
    // How one losing astrometry entered the winning coordinate model, so each
    // caller can diagnose a rejection with the labels of the records it merges.
    enum class AstrometryMergeOutcome {
        Adopted,
        MergedFields,
        RejectedAgainstFixed,
        RejectedAgainstReference
    };

    // Shared body of the takeHigherPrecedence overloads. `isAbsent` names the
    // value model's absent marker, so the winner's presence check and the
    // loser's skip check follow one rule for every value kind.
    template <typename T, typename IsAbsent>
    static void adoptHigherPrecedence(
        T& winnerValue,
        std::optional<CatalogFieldOrigins::ValueOrigin>& winnerOrigin,
        const T& loserValue,
        const std::optional<CatalogFieldOrigins::ValueOrigin> loserOrigin,
        const ValuePrecedence precedence,
        IsAbsent isAbsent
    )
    {
        if (isAbsent(loserValue)) {
            return;
        }
        Q_ASSERT(isAbsent(winnerValue) || winnerOrigin.has_value());
        Q_ASSERT(loserOrigin.has_value());
        if (!isAbsent(winnerValue) && !loserValueWins(*loserOrigin, *winnerOrigin, precedence)) {
            return;
        }

        winnerValue = loserValue;
        winnerOrigin = loserOrigin;
    }

    [[nodiscard]] static bool loserValueWins(
        const CatalogFieldOrigins::ValueOrigin& loserOrigin,
        const CatalogFieldOrigins::ValueOrigin& winnerOrigin,
        const ValuePrecedence precedence
    );

    [[nodiscard]] static bool loserCoordinateModelWins(
        const CatalogFieldOrigins& loserOrigins,
        const CatalogFieldOrigins& winnerOrigins,
        const ValuePrecedence precedence
    );

    [[nodiscard]] static AstrometryMergeOutcome mergeLosingAstrometry(
        OwnGalaxyCelestialBody& winner,
        CatalogFieldOrigins& winnerOrigins,
        const CatalogStarAstrometry& loserAstrometry,
        const CatalogFieldOrigins& loserOrigins,
        const ValuePrecedence precedence
    );

    static void replaceCoordinateModelInPlace(
        OwnGalaxyCelestialBody& winner,
        CatalogFieldOrigins& winnerOrigins,
        const BaseCelestialBody& loser,
        const CatalogFieldOrigins& loserOrigins,
        const ValuePrecedence precedence
    );

    static void logFixedCoordinateConflict(const BaseCelestialBody& kept, const BaseCelestialBody& conflicting);
    static void
    logRejectedAstrometry(const BaseCelestialBody& winner, const BaseCelestialBody& loser, const QString& keptModel);
    static void logRejectedFixedCoordinates(const BaseCelestialBody& winner, const BaseCelestialBody& loser);
    // Reports the diagnosis of one astrometry merge outcome: `kept` names the
    // record whose coordinate model survived and `rejected` the record whose
    // incompatible astrometry was dropped.
    static void logAstrometryMergeOutcome(
        AstrometryMergeOutcome outcome, const BaseCelestialBody& kept, const BaseCelestialBody& rejected
    );
};

}  // namespace skygate::ephemeris
