# Caching and performance

The application uses several lightweight caches instead of a global render
cache. Persistent catalog storage is covered in
[Catalog collection lifecycle](catalog-lifecycle.md).

## Snapshot cache

`SkySceneModel` caches `EphemerisSnapshot` by:

- catalog revision
- observer location
- UTC timestamp
- selected engine identity
- selected request epoch
- selected request options
- selected engine options revision
- ephemeris data revision
- Earth-orientation data revision
- leap-second data revision

If only view parameters change, the expensive ephemeris compute step is
skipped.

## Render-frame cache

`SkySceneModel` separately caches projected render output by:

- snapshot generation
- projection type
- viewport size
- view center
- field of view
- magnitude cutoff
- theme identity
- overlay layer visibility

This keeps pan/zoom/projection changes separate from catalog/time
recomputation.

## Prepared projection cache

`PreparedProjection` precomputes per-frame projection basis data and
constants. Each scene rebuild creates one from the current viewport and
view parameters, not per object. Direct projections and prepared
projections delegate to the same internal projection algorithms, keeping
test-only/direct paths numerically aligned with the render path.

## Large-catalog star decimation

`SkyRenderFrameBuilder` performs screen-space star decimation for dense
star catalogs. For large star catalogs and wider fields of view, only the
most relevant star per screen cell is kept, favoring brighter stars and
then proximity to the cell center.
