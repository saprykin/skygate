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
view parameters, not per object. The `ProjectionAlgorithms::project`
overload that takes a type and parameters runs the same code as the
prepared projection, so tests that project a single point stay
numerically aligned with the render path.

## Large-catalog star decimation

`SkyRenderFrameBuilder` performs screen-space star decimation for dense
star catalogs. For large star catalogs and wider fields of view, only the
most relevant star per screen cell is kept, favoring brighter stars and
then proximity to the cell center.

## High-precision engine caches

The high-precision engine keeps its own caches in
`libs/skygate-ephemeris`. `EphemerisComputationCache` stores completed
work, `EphemerisCacheGateway` is the read-through front end the engine
calls, and `FrameTransformMatrixCache` holds ERFA matrices for one frame
transform batch.

### Computation cache

`EphemerisComputationCache` implements `IEphemerisComputationCache` and
holds three bounded stores: whole `EphemerisSnapshot` values, prepared
request states, and single-body `CelestialBodyState` values. Each store
is an independent `KeyedLruCache`. The factory creates one cache per
high-precision engine, and the constructor argument sets the capacity of
all three stores. It defaults to eight entries and is clamped to at least
one.

An entry answers a request only after two checks: the hashed key must
match, and the stored identity must match the request field by field. The
identity covers the request epoch (both Julian-date parts and the time
scale), the UTC observation time at microsecond resolution, the observer
geodetic position, the engine options, the address of the engine's
catalog, and the data set description. The body-state store adds a body
index.

The engine options enter the identity as effective values: engine kind,
correction flags, fallback and refraction flags, pressure, temperature,
humidity, and observing wavelength. The specified-option mask is not
compared, so option sets that resolve to the same values share an entry.
The data set description covers id, display name, version, provenance,
and every date range with its id, display name, and endpoints.

The key string combines FNV-1a hashes of the request and the data set
with the catalog address. Comparing the stored identity after the hash
lookup means a collision produces a miss, never another request's result.
The epoch parts, observer coordinates, option doubles, and date-range
endpoints are compared at the bit level, so `-0.0` and `0.0` occupy
separate entries.

Each store records the insertion order of its keys and overwrites an
existing key in place. When a store grows past its capacity, it drops the
key inserted first. A read does not move an entry to the back, so despite
the `KeyedLruCache` name, eviction follows insertion order. A frequently
read entry can still be dropped by the next new key. The three stores
evict independently, so a snapshot and the prepared state that produced
it can leave at different times.

All public methods are `const`, and one mutex guards the three stores.
Every lookup, store, and `clear()` call takes that mutex with a
`std::scoped_lock`. The snapshot and body-state lookups copy their result
before releasing the lock, so the caller never holds a pointer into a
store. Traffic in one store delays access to the other two.

### Cache gateway

`HighPrecisionEphemerisEngine` reaches the cache through
`EphemerisCacheGateway`. The gateway is constructed with the cache, the
catalog address, and a copy of the data set description, so its identity
arguments are fixed and callers pass only a request. `snapshot()`,
`bodyState()`, and `preparedRequestState()` look up the value, run the
caller's callback on a miss, and store the result before returning it.
`findSnapshotBodyState()` only looks up, which lets a single-body call
reuse a state from a cached full snapshot without copying every state in
it.

A prepared request state holds the kernel epoch, the annual-parallax
Earth state, the Earth-orientation sample, and the topocentric observer
position built for one request, so separate body lookups reuse them
instead of repeating kernel and time-scale work.

A null cache pointer turns the gateway into a direct call: callbacks run
and nothing is stored. The factory gives every high-precision engine a
cache, and tests can build an engine without one.

The gateway holds no lock and does not coalesce misses. Thread safety
comes from the cache: each lookup and store takes the cache mutex, while
the callback runs outside it. Two threads that miss the same key both
compute and both store, and the last store wins.

### Frame transform matrix cache

`FrameTransformMatrixCache` belongs to `ErfaFrameTransformer`. Each call
to `transform()` creates a `FrameTransformSession` for the request epoch,
and the session owns the cache, so entries last for one batch and every
vector in that batch reuses them. The next call starts empty.

The key is the matrix kind named by a stage row:
`CelestialIntermediate`, `EarthRotation`, `PolarMotion`, or
`PrecessionNutation`. The cache is a fixed array with one slot per kind,
so it has no capacity setting and never evicts. The first stage that asks
for a kind computes its matrix, and later vectors read the same slot.

Each slot holds a matrix or its failure together with metadata. A
returned matrix contributes the slot's status and warnings to the calling
stage with `EphemerisMetadataMergeOptions::statusAndWarningsOnly()`, and
the caller marks that stage's correction as applied. A matrix that
cannot be computed stays in its slot as an empty value with the failure
recorded in its metadata, so the remaining vectors in the batch do not
retry it.

The session also caches the TT conversion used by the
celestial-intermediate, polar-motion, and precession-nutation matrices.
It also caches a UT1 conversion used when no Earth-orientation provider
is available, and shares one UTC conversion and one Earth-orientation
sample between the matrices that need polar motion or UT1-UTC. The
sample uses `EarthOrientationSampler::Options::tolerantFallback()`. A
failed conversion or sample stays in its slot and replays the same
failure metadata for the rest of the batch.

The matrix cache has no lock. It lives in the session created inside
`transform()` and never leaves that call, so two threads cannot reach the
same instance. `ErfaFrameTransformer` itself holds only the time-scale
and Earth-orientation provider pointers, so the engine can share it as
`shared_ptr<const ...>`.
