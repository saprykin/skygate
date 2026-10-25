#pragma once

#include "BaseCelestialBody.hpp"

#include <cstddef>
#include <optional>

namespace skygate::ephemeris {

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
struct CatalogFieldOrigins final {
    // Origin of one merged value: the rank of the source that supplied it and the
    // row order of the supplying record within that source. A value of a
    // higher-ranked source outranks a lower-ranked one; values that share a source
    // rank resolve by row order, where the first row of a source is authoritative
    // and its later rows only fill missing values.
    struct ValueOrigin final {
        std::size_t sourceRank = 0;
        std::size_t rowOrdinal = 0;
    };

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

    // True when the candidate origin outranks the current one: a higher source
    // rank, or an earlier row within the same source.
    [[nodiscard]] static bool outranks(const ValueOrigin& candidate, const ValueOrigin& current);

    // Origins of the values a freshly parsed record supplies: every present field
    // belongs to the source that owns the record, at that record's row order. The
    // absent markers of the value model decide presence: an empty display name is
    // absent, a NaN visual magnitude is absent, an empty deep-sky kind is absent,
    // and every optional value counts as soon as it has a value.
    [[nodiscard]] static CatalogFieldOrigins
    originsOf(const BaseCelestialBody& body, std::size_t sourceRank, std::size_t rowOrdinal);

    // Adopts the origins of one astrometry record: its anchor and every optional
    // astrometry field. A merge that adopts or replaces a coordinate model takes
    // the whole block, so no component origin outlives the value it describes.
    static void adoptAstrometryOrigins(CatalogFieldOrigins& target, const CatalogFieldOrigins& source);

    // Clears the astrometry anchor and every optional astrometry origin, for a
    // model whose astrometry was rejected as contradictory.
    static void clearAstrometryOrigins(CatalogFieldOrigins& target);

    // Adopts the origins of one deep-sky metadata block: its kind and every
    // optional size or orientation field.
    static void adoptDeepSkyOrigins(CatalogFieldOrigins& target, const CatalogFieldOrigins& source);

    // Origin of the survivor's coherent coordinate model anchor: the
    // higher-precedence origin among the model components the survivor carries, so
    // replacing or clearing a component never leaves a stale anchor behind.
    static void refreshCoordinateModelOrigin(CatalogFieldOrigins& origins);
};

}  // namespace skygate::ephemeris
