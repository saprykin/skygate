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
  - Engines own an immutable snapshot of catalog bodies so compute does not
    depend on external catalog lifetime.
- `IStarCatalog`:
  - Source abstraction for bundled and future external catalogs.
  - Exposes immutable body views instead of copying the full catalog on access.
- `SkySnapshot`:
  - Output payload containing computed celestial body states.
  - Shares immutable catalog bodies and stores state body indices instead of
    copying full body metadata into every frame.
- `CatalogLoadResult`:
  - Structured catalog load outcome with diagnostics and explicit error codes.
- `CatalogSelectionOptions`:
  - Optional caller-controlled selection policy for large imported catalogs.

## Planned Runtime Flow
1. UI gathers `GeoLocation`, current/specified UTC time, and projection
   settings.
2. UI builds `core::SkyContext`.
3. UI calls `ephemeris::IEphemerisEngine::compute`.
4. Snapshot body coordinates are projected through selected `core::IProjection`.
5. Render layer draws visible objects using Qt GPU-backed scene graph.
6. UI caches the last ephemeris snapshot and projected render frame separately
   so pan/zoom/hover reuse work instead of recomputing the full catalog.

## Extensibility Notes
- Projection strategies are isolated behind `IProjection`.
- `PreparedProjection` precomputes per-frame projection basis data so large
  render passes do not rebuild it for every star.
- Ephemeris computation remains UI-independent.
- Data sources can swap via `IStarCatalog` without changing UI code.
- Catalog rows support bundled defaults and runtime-downloaded datasets via the
  same `id|name|type|magnitude` row format.
- Runtime import supports major star catalogs in HYG CSV format (`ra`, `dec`,
  `mag`) and maps them to fixed-position stars.
- Large catalog selection is explicit at load time instead of being baked into
  the raw HYG parser.
- Body computation uses explicit ephemeris source metadata rather than inferring
  runtime behavior inside the engine from mixed `type` and `id` checks.
- UI preset `hyg_v3` also attempts to load Stellarium skyculture line data
  (`western/index.json`, HIP-based) to render expanded constellation outlines.
- Large downloaded catalogs stay intact at load time, while the render layer
  applies screen-space star decimation when density would otherwise overwhelm
  frame time.
