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

- `IProjection`: runtime strategy interface for projections.
- `createProjection(ProjectionType)`: factory for selecting a concrete
  projection implementation.
- Concrete strategies: `StereographicProjection`,
  `AzimuthalEquidistantProjection`, `PerspectiveProjection`.
- `PreparedProjection`: per-frame precomputation object used by the render
  path. Stores the normalized center basis and projection-specific
  constants so large render passes do not repeat setup work for every star.
- `ProjectionAlgorithms`: internal implementation shared by the direct
  projection strategies and `PreparedProjection`, so projection formulas
  have one behavioral source.
- `ProjectionPipeline`: internal helper that turns normalized projection
  results into `ScreenPoint` status and viewport coordinates.
- Focused geometry helpers:
  - `Geometry2d` for primitive 2D math, rectangles, and line segments
  - `RectOccupancyGrid` and `CircleHitIndex` for screen-space indexes
  - `DashedLineBuilder` for dash generation
  - `ProjectedPolylineBuilder` for projection-aware polyline splitting
- `SphericalGeometry`: spherical/vector helpers shared by projection and
  ephemeris code. It converts horizontal coordinates to and from unit
  vectors, builds a projection basis, rotates a vector around an axis,
  measures the angular separation of two directions, and interpolates
  between directions along the chord between their unit vectors. A rotated
  or interpolated vector that collapses to zero length falls back to the
  zenith direction.

`PreparedProjection` is the preferred high-throughput path in the current
UI. The `IProjection` strategies still exist as a clean abstraction
boundary and are created by the controller when the projection type is
selected.

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
