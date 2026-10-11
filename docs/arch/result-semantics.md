# Result semantics

`CelestialBodyState` carries a body index, an `EquatorialCoordinate`
(right ascension/declination), a `HorizontalCoordinate`
(altitude/azimuth), and an `EphemerisEngineQueryResult` metadata block.
The meaning of the coordinates depends on the engine and on the requested
corrections.

## Simple engine

- Fixed catalog bodies (stars, constellations, and deep-sky objects) use
  their catalog fixed-equatorial coordinates unchanged; no precession,
  nutation, proper motion, or parallax is applied.
- The Sun, Moon, and planets use approximate orbital elements of date
  converted to equatorial coordinates through the mean obliquity of date.
- Horizontal coordinates are derived from the equatorial position using
  Greenwich mean sidereal time and the observer's geodetic position. No
  atmospheric refraction or diurnal parallax is applied.
- The simple engine applies at most the `geometric` correction; any other
  requested correction terms are reported as unavailable and the result is
  marked degraded. An invalid observer degrades the result and leaves
  horizontal coordinates unset (`NaN`).

## High-precision engine

- Base solar-system and star positions are computed geocentrically in the
  GCRS (geocentric celestial reference system) frame.
- With no corrections requested, `equatorial` is the geometric GCRS
  position. Astrometric terms (proper motion, parallax, radial velocity,
  annual parallax) are folded into the GCRS direction when requested and
  available.
- When precession/nutation is requested, `equatorial` is transformed to
  the true equator and equinox of date (apparent place).
- When diurnal parallax (topocentric) is requested, the position is
  transformed to ITRS, the observer parallax is applied, and `horizontal`
  is populated as the topocentric altitude/azimuth. Atmospheric refraction
  is applied to `horizontal` when it is both requested and enabled.
- `horizontal` is unset (`NaN`) unless the topocentric path runs;
  `equatorial` reflects geometric/astrometric GCRS, apparent of date, or a
  topocentric direction, depending on the requested corrections.

## Unknown bodies versus failed calculations

- The single-body overloads return `std::nullopt` when the body cannot be
  found (unknown id or out-of-range index). This is distinct from a failed
  or unsupported calculation, which returns a state.
- In a full snapshot every catalog body receives a state entry. A body the
  engine cannot compute is reported as
  `EphemerisEngineQueryStatus::Unsupported` (with the `UnsupportedBody`
  warning) and `NaN` coordinates; a failed calculation is reported as
  `EphemerisEngineQueryStatus::Failed` (with the `ComputationFailed`
  warning).

## Supported date ranges

`supportedDateRanges()` declares the validity window of the engine's
underlying data set. The simple engine has no time-bounded data and
returns an empty range; the high-precision engine returns the ranges
declared by its active data set. These ranges are advisory: requests
outside them are not rejected at the interface but produce out-of-range or
degraded metadata (for example the `DataOutOfRange` warning), and runtime
fallback may substitute the simple engine when the caller allows it.

## Thread safety of const methods

Shared const ownership (`std::shared_ptr<const IEphemerisEngine>`) does not
by itself make concurrent const calls safe; an implementation with mutable
caches must synchronize them internally. The simple engine holds only
immutable state (its catalog copy, options, and stateless calculators), so
its const methods are reentrant. The high-precision engine is documented
as safe for concurrent read-only computations after construction: its only
mutable state is the `EphemerisComputationCache`, whose accessors are
mutex-guarded, and mutable data updates are expected to create a new
engine instance (or a versioned immutable snapshot) rather than mutate
active computation data in place.
