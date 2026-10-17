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

The selected epoch and request options enter the key through the equality of
`AstronomicalEpoch` and `EphemerisEngineOptions`, so every field those types
compare participates without a second list. Option sets with equal effective
values compare equal even when the specified-option mask differs. The trail
sample cache in `SkyObjectTrailSampler` applies the same rule and also keys
the samples by engine, guidance strategy, observation context, target index,
and target equatorial coordinates. The cached window spans the whole trail,
so projection, viewport, and theme changes reuse it.

The engine-option revision belongs to `SkyEphemerisSession`, which advances
it when the selected kind or options change. The session also owns the
backend-resource cache: the cache keeps the time-scale, Earth-orientation,
and CALCEPH kernel providers between rebuilds and reloads them when the
active data snapshot, manifest, or data revision changes.

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

## Viewport snapshot

`SkySceneModel` publishes one immutable `SkyViewportSnapshot` after a
rebuild that changed the render frame or the active trail target. The
publication key pairs the render-frame generation from the pipeline with
the active trail target, so
an inspector drag, pin toggle, or inspector text update reuses the current
snapshot. Projection, viewport size, view center, field of view, magnitude
cutoff, theme, and layer visibility changes advance the render-frame
generation; catalog, location, engine, and time changes advance the
snapshot generation that feeds it. The snapshot carries the prepared
projection, the resolved observation context, the render palette, layer
visibility, and the render point, line, and glyph vectors, so the viewport
builds geometry only when it receives a new one.

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
