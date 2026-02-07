# Skygate Architecture (Initial)

## Module Layout
- `apps/skygate-ui`: Qt Quick application shell and user interaction.
- `libs/skygate-core`: domain contracts for time/location/projection and shared
  types.
- `libs/skygate-ephemeris`: astronomy computation contracts and catalog
  integration.

## API Contracts

### `skygate-core`
- `GeoLocation`:
  - Observer coordinates and elevation.
  - `isValid()` for bounds and finite value checks.
- `SkyContext`:
  - Full input context for sky computation.
  - Includes observer location and UTC timestamp.
- `IProjection`:
  - Projection abstraction to support future projection types.
  - `project(HorizontalCoordinate, ProjectionParams)` returns a visibility-aware
    screen point.
- `ITimeSource`:
  - Injectable time provider for deterministic tests and live mode.

### `skygate-ephemeris`
- `IEphemerisEngine`:
  - Single entry-point for sky computations.
  - `compute(SkyContext)` returns `SkySnapshot`.
- `IStarCatalog`:
  - Source abstraction for bundled and future external catalogs.
- `SkySnapshot`:
  - Output payload containing computed celestial body states.

## Planned Runtime Flow
1. UI gathers `GeoLocation`, current/specified UTC time, and projection
   settings.
2. UI builds `core::SkyContext`.
3. UI calls `ephemeris::IEphemerisEngine::compute`.
4. Snapshot body coordinates are projected through selected `core::IProjection`.
5. Render layer draws visible objects using Qt GPU-backed scene graph.

## Extensibility Notes
- Projection strategies are isolated behind `IProjection`.
- Ephemeris computation remains UI-independent.
- Data sources can swap via `IStarCatalog` without changing UI code.
- Catalog rows support bundled defaults and runtime-downloaded datasets via the
  same `id|name|type|magnitude` row format.
- Runtime import supports major star catalogs in HYG CSV format (`ra`, `dec`,
  `mag`) and maps them to fixed-position stars.
