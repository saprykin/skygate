#pragma once

#include "EquatorialCoordinate.hpp"
#include "catalog/CatalogStarAstrometry.hpp"

namespace skygate::ephemeris {

// Coherence rules for a body's coordinate description.
//
// A body describes its position either through a fixed position, through
// astrometry (reference position, reference epoch, and optional motion and
// validity), or through both. Two positions describe the same direction when
// they agree within a small angular tolerance; independent catalogs place a
// matched object well inside that bound, so a larger separation means
// unrelated coordinate models rather than catalog noise.
//
// Astrometry from two records may describe the same object at different
// reference epochs. Their reference positions are therefore compared after
// the losing proper motion accounts for the epoch difference, but only when
// both records declare a reference epoch: an undeclared epoch claims the same
// epoch as the record it is compared with, so no epoch conversion applies.
// The stored right-ascension rate is the tangent-plane component
// mu_alpha * cos(delta) in mas/year, so it converts directly to an angular
// offset over time.
//
// A fixed position carries no reference epoch, so a comparison against one
// applies no epoch conversion: a fixed position and a reference position only
// describe the same direction when their values already agree.
class CatalogCoordinateModel final {
public:
    [[nodiscard]] static bool sameDirection(
        const skygate::core::EquatorialCoordinate& lhs, const skygate::core::EquatorialCoordinate& rhs
    ) noexcept;

    [[nodiscard]] static bool
    sameAstrometry(const CatalogStarAstrometry& winner, const CatalogStarAstrometry& loser) noexcept;
};

}  // namespace skygate::ephemeris
