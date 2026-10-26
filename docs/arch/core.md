# skygate-core module

`skygate-core` provides stable, UI-independent core types and projection
logic.

## Core types

- `GeoLocation`
- `UtcTimePoint`
- `EquatorialCoordinate`
- `HorizontalCoordinate`
- `ObservationContext`
- `ProjectionType`
- `ProjectionParams`
- `ScreenPoint`

These are small value types so they can move cheaply across module
boundaries.

## Projection subsystem

- `ProjectionType`: selects stereographic, azimuthal equidistant, or
  perspective.
- `ProjectionParams`: view center, field of view, roll, and viewport size.
- `PreparedProjection`: per-frame precomputation object used by the render
  path. `PreparedProjection::create` checks the parameters, normalizes the
  view center, and stores the center basis and projection constants in a
  `ProjectionFrame`, so large render passes do not repeat setup work for
  every star. `project` and `projectWithMargin` reuse that frame.
- `ProjectionAlgorithms`: internal implementation behind the prepared
  projection. `prepareFrame` fills the `ProjectionFrame`, and `project`
  applies the projection formula, roll, and visibility rules.
- `ProjectionPipeline`: internal helper that scales a projected position
  into viewport pixels and culls it outside the field of view, using a
  circular bound for the azimuthal projections and a rectangular bound for
  the perspective one.

The UI builds one `PreparedProjection` per frame and projects every body
through it. `ProjectionAlgorithms` holds the only copy of each projection
formula; tests call it directly to check the formulas without building a
prepared frame.

## Geometry helpers

These helpers back the projection and render paths:

- `Geometry2d` for primitive 2D math, rectangles, and line segments
- `RectOccupancyGrid` and `CircleHitIndex` for screen-space indexes
- `DashedLineBuilder` for dash generation
- `ProjectedPolylineBuilder` for projection-aware polyline splitting. It
  splits each segment according to the angular distance between its
  endpoints and the field of view, then resamples through
  `SphericalGeometry`.
- `SphericalGeometry`: spherical/vector helpers shared by projection and
  ephemeris code. It converts horizontal coordinates to and from unit
  vectors, builds a projection basis, rotates a vector around an axis,
  measures the angular separation of two directions, and interpolates
  between directions along the great circle, using arc-length weights and
  falling back to a normalized chord when the directions are nearly
  collinear. A rotated or interpolated vector that collapses to zero length
  falls back to the zenith direction.

## Time abstraction

- `ITimeSource`: small interface that makes time acquisition replaceable
  and testable.
- `SystemTimeSource`: default production implementation.
- `UtcTimePoint` / `UtcTimeCodec`: microsecond UTC timestamp storage and
  epoch-scalar conversion.
- `TimeScale`: UTC/TAI/TT/TDB/UT1 enum used across time-scale conversion
  boundaries.
- `CivilDateTime` / `CalendarTime`: proleptic Gregorian civil-date
  validation and civil-date/epoch mapping.
- `AstronomicalEpoch`: two-part Julian-date value with normalization,
  arithmetic, and ordering.
- `EpochCodec`: UTC time to Julian-date/J2000 epoch conversion.
- `AstronomicalTime`: mean obliquity and Greenwich mean sidereal time
  helpers.

Calendar and Julian-date arithmetic lives in `skygate-core` so both the
simple and high-precision engines share one time implementation.
