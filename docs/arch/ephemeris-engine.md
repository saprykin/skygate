# skygate-ephemeris module

`skygate-ephemeris` owns celestial body metadata, catalog parsing, and
runtime sky computation. Catalog parsing and composition are described in
[Catalog composition](catalog-composition.md); this file covers the catalog
and snapshot models and the ephemeris engines.

## Catalog model

- `IStarCatalog`: abstract read-only catalog interface.
- `InMemoryStarCatalog`: current concrete catalog implementation backed by
  a `CelestialBodyCatalog`.
- `BaseCelestialBody`: polymorphic base for body metadata. Owns the common
  id, display name, body kind, and magnitude fields, and exposes virtual
  accessors for optional fixed coordinates, star astrometry, and deep-sky
  metadata.
- `OwnGalaxyCelestialBody` / `DistantCelestialBody`: concrete body storage
  for own-galaxy and distant objects.
- `CelestialBodyCatalog`: owns separate concrete body vectors and a stable
  ordered pointer view used by engines, snapshots, search, and rendering.

## Snapshot model

- `IEphemerisEngine`: pure interface that computes an `EphemerisSnapshot`
  from a `core::ObservationContext` and resolves individual body states.
- `EphemerisEngineQueries`: query helper that routes lookups through the
  engine's direct single-body virtual overloads, with snapshot-scan helpers
  for finding a body state inside an already-computed snapshot.
- `EphemerisSnapshot`: current context, shared immutable catalog body
  vector, and per-frame body states that reference catalog bodies by index.

The immutable/shared snapshot avoids copying full body metadata into each
frame and keeps rendering decoupled from catalog ownership.

## Request time contract

`EphemerisRequest` carries an explicit astronomical epoch and a UTC time in
its observation context. For request-based engine calls the explicit
`request.epoch` is the single authoritative computation instant; engines
must not substitute `request.context.utcTime` for a valid explicit epoch.
The observation-context convenience overloads carry no explicit epoch: they
build an `EphemerisRequest` from the engine's current options and a UTC
epoch derived from `context.utcTime`
(`EphemerisRequestFactory::requestFromContext`), so that derived UTC epoch
is the authoritative instant for those calls. Callers that need a non-UTC
epoch or explicit correction control build an `EphemerisRequest` directly.

Engines enforce this contract according to their capabilities. The simple
engine is UTC-only: when it receives an explicit non-UTC epoch it reports
each body as unsupported (`EphemerisEngineQueryStatus::Unsupported` with
the `UnsupportedTimeScaleConversion` warning) instead of silently using
`context.utcTime`. The high-precision engine honors explicit epochs: it
rejects a non-explicit (default/zero) epoch as a failed computation, uses
TDB epochs directly for kernel queries, and converts other scales through
its injected time-scale service, recording any conversion metadata.

## Engines

Engines are created through `EphemerisEngineFactory`.

### SimpleEphemerisEngine

The always-available engine keeps an immutable `CelestialBodyCatalog`,
dispatches coordinate generation by `BaseCelestialBody::Kind` and available
fixed-equatorial catalog coordinates, and delegates to focused calculators
and lookups: `SunEquatorialCalculator`, `MoonEquatorialCalculator`,
`PlanetEquatorialCalculator`. It consumes catalog-owned `fixedEquatorial`
coordinates for fixed catalog bodies (stars, constellations, deep sky
objects), converts ecliptic to equatorial coordinates via
`EclipticToEquatorialCalculator`, converts equatorial to horizontal
coordinates via `EquatorialToHorizontalCalculator`, and relies on the shared
`skygate-core` helpers: `EpochCodec` for UTC time to Julian date and J2000
conversion, and `AstronomicalTime` for mean obliquity and Greenwich mean
sidereal time.

### HighPrecisionEphemerisEngine

With `SKYGATE_ENABLE_HIGH_PRECISION_EPHEMERIS=ON` the factory can also
create `HighPrecisionEphemerisEngine`, a facade over focused high-precision
providers and calculators:

- `CalcephKernelProvider`
- `SolarSystemStateCalculator`
- `StarAstrometryCalculator`
- `LeapSecondTimeScaleService`
- `IEarthOrientationProvider`
- `TableBackedEarthOrientationProvider`
- `EarthOrientationDataParser`
- `EarthOrientationDataLoader`
- `EarthOrientationSampler`
- `ErfaFrameTransformer`
- `ApparentPlaceCalculator`
- `AtmosphericRefractionCalculator`
- `EphemerisComputationCache`

High-precision pipeline contracts have individual headers and companion
sources: `HighPrecisionComputationInput`, `HighPrecisionCalculatorResult`,
`PreparedEphemerisRequestState`, and `StarAstrometryBatchResult`. Kernel
state results belong to `ICalcephKernel::StateResult`; engine construction
uses `HighPrecisionEphemerisEngine::Dependencies`. Consumers include the
specific contracts they use.

Earth-orientation parsing returns validated rows and metadata without
constructing a provider. The loader obtains snapshot assets, applies
reference-epoch staleness, and creates an immutable table-backed provider.
The separate sampler resolves UT1-UTC and polar motion using caller-owned
fallback options and reports sample warnings. Provider contract types are
nested in the interface; parsing, loading, and sampling types are nested in
their respective classes.

Ephemeris data updates use two independent library operations.
`EphemerisStagedUpdateVerification::verify()` checks the staged profile,
component metadata, and payloads without activating files.
`EphemerisDataActivation::activate()` writes a validated asset into the
cache and promotes it atomically. Each class has separate request, result,
and status types. Both use `EphemerisDataPayloadReader` for streaming
decompression and checksums; their operation-specific helpers stay in their
source files.

Manifest assets with `live: true` describe upstream files that may change
without a manifest update (for example the IERS Earth-orientation series).
Live assets may omit checksum and size metadata; verification and
activation still require a readable non-empty payload and a valid manifest
validity range, but they do not enforce exact size or checksum equality.
Pinned (non-live) assets, including all solar-system kernels, keep the
strict size and sha256 checks.

The factory supports strict high-precision creation or simple-engine
fallback through `EphemerisFactoryFallbackPolicy`. `SkyContextController`
owns the selected engine kind, correction options, refraction settings,
data manager, and diagnostics surfaced to the UI.

## High-precision interface layer

Replaceable high-precision subsystems are exposed as narrow `I*` contracts
so the engine and factory depend on behavior rather than concrete
providers:

- `ICalcephKernel` / `ICalcephKernelProvider` for kernel state and kernel
  acquisition
- `IDeltaTProvider`, `ILeapSecondProvider`, and `IEarthOrientationProvider`
  for time-scale support tables
- `ITimeScaleService` for UTC/TAI/TT/TDB/UT1 conversion
- `ISolarSystemStateCalculator` and `IStarAstrometryCalculator` for body
  state and catalog-star astrometry
- `IApparentPlaceCalculator` and `IAtmosphericRefractionCalculator` for
  apparent/topocentric corrections
- `IEphemerisComputationCache`, `IEphemerisResultBuilder`, and
  `IFrameTransformer` for caching, result assembly, and frame transforms
- `IEphemerisFallbackStrategy` for simple-engine degradation
- `IEphemerisDataSnapshot` for staged high-precision data assets

Concrete implementations (`CalcephKernel`, `TableBacked*Provider`,
`LeapSecondTimeScaleService`, the calculators, `EphemerisComputationCache`,
`ErfaFrameTransformer`, and `SimpleEphemerisFallbackStrategy`) are wired
through `EphemerisEngineFactory` and injected as `shared_ptr<const ...>`
dependencies.

## Fallback

Fallback is implemented once in the simple-engine module as
`SimpleEphemerisFallbackStrategy`, injected into the high-precision engine
through `IEphemerisFallbackStrategy`. When a high-precision request is out
of range and the request explicitly allows fallback, the strategy produces
a simple-engine body state, marks the high-precision result as degraded,
and records the corrections that could not be applied as unavailable.

Creation and runtime degradation use separate policies.
`EphemerisEngineFactoryRequest::fallbackPolicy` governs whether factory
creation may substitute the simple engine for a requested high-precision
engine. It defaults to `AllowSimpleEngineFallback`, so a default
high-precision request keeps the existing fallback behavior. Setting it to
`StrictHighPrecision` makes creation fail with
`FailedStrictHighPrecisionUnavailable` when high-precision dependencies are
unavailable, regardless of the runtime option.
`EphemerisEngineOptions::fallbackToSimpleEngine()` only governs runtime
degradation inside an already-created high-precision engine; its default
stays `true`.

### Guidance sampling

Event and trail sampling use an explicit `IEphemerisGuidanceStrategy`.
`SimpleEphemerisGuidanceStrategy` creates an inexpensive simple engine for
guided event search and UI guidance trails. This is a separately selected
sampling path, independent of factory creation fallback and runtime
degradation: turning off fallback does not remove guidance, and guidance
samples do not imply a degraded high-precision result.

## Requested versus effective engine

`EphemerisEngineFactoryResult` distinguishes the engine the caller asked
for from the engine actually produced. `requestedKind()` is the kind passed
to the factory; `effectiveKind()` is the kind of the returned engine (or
the requested kind when creation fails). A high-precision request whose
dependencies are unavailable therefore succeeds with
`requestedKind() == EphemerisEngineKind::Type::HighPrecision`, a `Simple`
engine, and `EphemerisFactoryCreationStatus::CreatedSimpleFallback`, which
`usedSimpleEngineFallback()` reports.

`SkyContextController` uses that distinction when it rebuilds the active
engine. If a rebuild would replace an active high-precision engine with a
simple fallback, the controller keeps the active engine instead of applying
the lower-fidelity result. The rule: a degraded fallback never silently
replaces a more capable active engine. The active engine remains in effect
until a rebuild produces an engine at the requested capability.
