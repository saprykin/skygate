# High-Precision Ephemeris Engine PRD

Status: implementation baseline plus remaining product requirements

Last updated: 2026-05-28

## Purpose

SkyGate has a simple ephemeris engine for lightweight visual placement and a
high-precision engine path behind `SKYGATE_ENABLE_HIGH_PRECISION_EPHEMERIS`.
This document records the product and technical requirements for the
high-precision path and separates the implemented baseline from remaining
release-hardening work.

The target is sub-arcsecond apparent RA/Dec where the underlying data supports
it, with milliarcsecond-class internal algorithms for modern dates when
possible. The engine must also provide a long-range mode covering approximately
year -13200 to +13200 with explicit degradation warnings where physical data or
time-scale data are uncertain.

## Field Research Summary

The high-precision engine must be based on established astronomical standards
and data products rather than locally derived approximations.

- Fundamental astronomy algorithms must follow IAU SOFA/IERS conventions for
  time scales, reference frames, precession, nutation, polar motion, Earth
  rotation, and star catalog transformations. SOFA is the authoritative
  reference implementation maintained under the IAU; ERFA is a BSD-licensed
  SOFA-derived implementation suitable for open-source integration if a vcpkg
  or overlay-port route is chosen.
- Solar-system positions must come from JPL Development Ephemerides through
  CALCEPH. CALCEPH is available as the `calceph` vcpkg port and reads binary
  planetary ephemeris files including SPK/JPL ephemerides.
- The optional long-range kernel target is DE441 because it covers year -13200
  to +17191. DE440 is more accurate for the modern interval but covers only
  1550-2650. Because DE441 is multiple gigabytes, it must not be part of the
  default bundled install. The default release data set should bundle a
  practical modern kernel, such as DE440 or DE440s, and make DE441 available as
  an optional Preferences download for users who need the full historical range.
- JPL Horizons is the primary reference service for validation fixtures. Test
  vectors should request ICRF vectors with no apparent corrections for geometric
  kernel validation, and separately request observer/apparent quantities for
  end-to-end apparent-position validation.
- Accurate topocentric apparent positions require more than planetary kernels:
  leap seconds, UTC/TAI/TT/TDB conversion, UT1-UTC, polar motion, precession,
  nutation, aberration, light-time, gravitational light deflection, parallax,
  and optionally atmospheric refraction.

Primary references:

- [IAU SOFA](https://www.iausofa.org/)
- [SOFA overview](https://www.iausofa.org/about-us)
- [IERS Conventions](https://www.iers.org/IERS/EN/DataProducts/Conventions/conventions)
- [IERS Conventions 2010 Technical Note 36](https://www.iers.org/IERS/EN/Publications/TechnicalNotes/tn36)
- [JPL DE440/DE441 document](https://ssd.jpl.nasa.gov/doc/de440_de441.html)
- [JPL Horizons manual](https://ssd.jpl.nasa.gov/horizons/manual.html)
- [CALCEPH vcpkg port](https://vcpkg.io/en/package/calceph.html)
- [zstd vcpkg port](https://vcpkg.io/en/package/zstd.html)
- [ERFA project](https://github.com/liberfa/erfa)

## Current State

The public include root is `libs/skygate-ephemeris/src`, following the
repository convention that headers live beside their `.cpp` files. The public
engine interface is `IEphemerisEngine.hpp`. It exposes engine metadata,
capabilities, date ranges, active data-set information, options, request-based
compute overloads, and compatibility overloads using
`core::ObservationContext`.

The simple implementation lives in `src/engine/simple/`. It supports fixed
catalog coordinates and approximate Sun, Moon, and planet positions.
Unsupported bodies return invalid/degraded states where appropriate.

When high precision is enabled, `EphemerisEngineFactory` can create a
`HighPrecisionEphemerisEngine` from `EphemerisEngineFactoryRequest`. The
request wires the selected engine kind, catalog, options, data manifests,
active data snapshot, time-scale service, Earth-orientation provider, CALCEPH
runtime, fallback policy, and diagnostics sink.

The high-precision implementation lives in `src/engine/highprecision/` and is a
facade over CALCEPH solar-system state, star astrometry, time-scale conversion,
Earth-orientation data, frame transforms, apparent-place corrections,
atmospheric refraction, and computation caching.

The UI owns engine selection through `SkyContextController`. Catalog state is
owned by `SkyCatalogManager`; ephemeris data state is owned separately by
`SkyEphemerisDataManager`. Scene cache invalidation includes selected engine
identity, engine options, and ephemeris data revision.

The app currently bundles the ephemeris data manifest as a Qt resource. Kernel,
Earth-orientation, leap-second, and Delta T assets are described by that
manifest and are installed/downloaded as external cache files rather than
embedded in the resource system.

Known limitations:

- Long-range DE441-size data remains optional and cannot be bundled by default.
- Full product validation still needs broad Horizons fixtures and
  release-platform package smoke coverage.
- Ancient apparent positions remain limited by Delta T, Earth rotation models,
  historical calendar interpretation, and catalog quality.
- Update UX and packaged data policies need continued release hardening.

## Goals

- Provide a high-precision engine selectable alongside the existing simple
  engine.
- Compute geometric, astrometric, and apparent RA/Dec for all supported objects.
- Compute topocentric apparent positions with optional parallax and refraction
  when observer information is available.
- Support year -13200 to +13200 when data permits, with adaptive accuracy and
  explicit warnings outside reliable data intervals.
- Use CALCEPH through vcpkg for planetary/lunar kernels.
- Bundle required kernels and time data with the app in compressed form.
- Allow in-app updates of kernels, EOP data, and leap-second data from
  Preferences.
- Keep offline operation fully supported using bundled data.
- Make the ephemeris engine the single point of truth for sky-object position
  calculations.
- Optimize for full-frame rendering and search across tens of thousands of
  objects.
- Add extensive deterministic tests with reference data in the repository,
  using Git LFS for large fixtures and kernels.

## Non-Goals

- The engine will not numerically integrate planetary ephemerides itself.
- The first high-precision release will not attempt high-precision arbitrary
  asteroid/comet orbit propagation unless the required SPK data is available.
- The engine will not own network UI. Download/update orchestration belongs in
  the application layer.
- The engine will not silently extrapolate high-precision claims outside the
  validity range of its data.

## Product Requirements

### Engine Selection

- Preferences must expose exactly two engine modes at the top level:
  `Simple` and `High precision`.
- `Simple` preserves the current lightweight behavior and remains available as a
  fallback.
- `High precision` exposes additional options for apparent corrections and data
  management.
- Engine choice and high-precision options must be persisted through
  `SkySettingsStore`.
- The selected engine must be applied consistently to frame rendering, search,
  tracking, object inspection, trails, and any other sky-object position
  calculation.

### Accuracy and Ranges

- The preferred high-precision target is milliarcsecond-class internal
  calculations for modern dates where source data and time data support it.
- The required user-visible target is below one arcsecond for major solar-system
  objects and catalog stars where the data supports it.
- The high-precision engine must report its supported date ranges:
  - full requested product range: year -13200 to +13200
  - modern high-accuracy range: determined by bundled modern kernels, typically
    DE440 or DE440s where available
  - EOP-supported range
  - leap-second-table range
  - catalog astrometry reference/range where relevant
- For ancient dates, the engine must use the best available assumptions and mark
  results with accuracy/degradation warnings.
- Out-of-range requests must return degraded fallback results where possible,
  not hard fail, and must provide a warning suitable for UI tooltip display.

### Correction Options

High precision mode must support caller-selectable correction flags:

- light-time correction
- stellar aberration
- gravitational light deflection
- annual parallax
- diurnal/topocentric parallax
- precession and nutation
- Earth orientation correction using UT1-UTC and polar motion
- atmospheric refraction
- proper motion, radial velocity, and stellar parallax for catalog stars

The UI should present common presets, but the engine API must allow explicit
flags so internal callers can request geometric, astrometric, or apparent
results.

### Result Status

Every high-precision result must expose:

- computation status: valid, degraded, unsupported, out of range, or failed
- warning codes suitable for UI display
- human-readable warning text
- data source provenance
- effective data validity range
- estimated angular uncertainty when available
- correction flags actually applied

`CelestialBodyState` carries `EphemerisEngineQueryResult` metadata with status,
warning mask, provenance, validity range, uncertainty, and correction tracking.

## Architecture Requirements

### Source Layout

Engine implementations live under dedicated subfolders:

- `libs/skygate-ephemeris/src/engine/simple/`
- `libs/skygate-ephemeris/src/engine/highprecision/`
- shared low-level helpers under `libs/skygate-ephemeris/src/engine/common/`
  only when used by both engines

Public headers remain under `libs/skygate-ephemeris/src`, not a separate
`include/` tree.

### Public API

The public API includes:

- `EphemerisEngineKind`
- `EphemerisEngineOptions`
- `EphemerisCorrectionFlags`
- `EphemerisCapabilities`
- `EphemerisDateRange`
- `EphemerisDatasetInfo`
- `EphemerisEngineWarning`
- `EphemerisEngineQueryStatus`
- `EphemerisEngineQueryResult`
- `EphemerisPrecisionPolicy`
- `EphemerisRequest`
- `AstronomicalEpoch`
- `TimeScale`

`IEphemerisEngine` exposes:

- `kind()`
- `name()`
- `capabilities()`
- `supportedDateRanges()`
- `dataSetInfo()`
- `options()`
- `compute(const EphemerisRequest&)`
- `computeBodyState(const EphemerisRequest&, std::string_view id)`
- `computeBodyState(const EphemerisRequest&, std::size_t index)`

The `core::ObservationContext` overloads are compatibility adapters. They
construct a default apparent/topocentric request using the engine's configured
options.

### Factory

`EphemerisEngineFactory` is responsible for engine kind selection and data
wiring.

Required creation inputs:

- engine kind
- catalog bodies
- engine options
- kernel/data-set manifest
- active data snapshot
- time-scale service
- Earth-orientation provider
- optional CALCEPH runtime
- optional logger/diagnostics sink

The factory must return a simple engine when high-precision dependencies or data
are unavailable and the caller requests fallback behavior. It must return a
structured error when strict high-precision creation is requested and cannot be
satisfied.

### High-Precision Engine Facade

The high-precision `IEphemerisEngine` implementation must remain a facade over
focused calculator/provider classes:

- `CalcephKernelProvider`
- `SolarSystemStateCalculator`
- `StarAstrometryCalculator`
- `TimeScaleService`
- `EarthOrientationProvider`
- `ErfaFrameTransformer`
- `ApparentPlaceCalculator`
- `AtmosphericRefractionCalculator`
- `EphemerisResultBuilder`
- `EphemerisComputationCache`

The facade owns configuration, validates requests, coordinates calculators, and
assembles result status/provenance. Calculator classes should remain testable
without UI.

### Time Model

Do not extend `ITimeSource` into a full astronomy service. The existing
`ITimeSource` should remain a core clock interface for "what time is it now?"
and continue to support test injection.

Add ephemeris-specific time types and services:

- `AstronomicalEpoch`: two-part Julian date plus time scale
- `CivilDateTime`: signed astronomical year, month, day, time, subsecond
- `TimeScaleService`: UTC, TAI, TT, TDB, UT1 conversion
- `LeapSecondProvider`: table-backed UTC/TAI data
- `DeltaTProvider`: historical and future Delta T estimates
- `EarthOrientationProvider`: UT1-UTC, polar motion, EOP interpolation

Reasoning:

- `core::UtcTimePoint` cannot represent the full requested date range robustly
  across all platforms.
- Leap seconds such as `23:59:60` cannot be represented by the current Qt
  `QTime` parsing path.
- Planetary kernels use ephemeris time scales, not raw UTC epoch seconds.
- Observer-frame apparent positions require UT1 and polar motion.

The UI should continue to use `ITimeSource` for live current time. Conversion
from UI date/time to `AstronomicalEpoch` must happen before calling the
high-precision engine.

### Data Management

`SkyEphemerisDataManager` owns bundled manifest state and installed
ephemeris-data cache state in the UI layer. It reports kernel,
Earth-orientation, leap-second, and Delta T status, stages verified updates,
activates snapshots atomically, and emits data revision changes that invalidate
engines and scene caches.

Current app packages bundle the manifest. Kernel/support assets are resolved
from external app resources or installed cache, so clean offline high-precision
operation requires packaged external data or prior installation:

- DE440 or DE440s modern kernel when package size allows modern higher accuracy
- leap-second table
- Earth-orientation data with a recent prediction interval
- Delta T model/table for ancient dates
- manifest with data versions, source URLs, checksums, validity ranges, and
  compression metadata

Make multi-gigabyte long-range kernels optional downloads:

- DE441 must be available through Preferences for users who need year -13200 to
  +13200 coverage.
- The app must clearly show whether long-range data is installed.
- If DE441 is not installed, dates outside the bundled kernel range must use a
  degraded fallback where possible and show a warning with tooltip text.
- The download flow supports checksum verification, restartable staging, atomic
  activation, and safe cancellation. Resumable range downloads are not
  implemented yet.

Large data assets must not be placed in Qt resource files. Keep the manifest in
resources, and install or download kernels/support data as external files. When
assets are distributed as archives, use zstd-capable packaging and activate the
verified files atomically in the writable app data location.

Use Git LFS for large repository assets:

- reference fixtures
- bundled kernel archives when they must live in the repository
- large generated validation data

### In-App Updates

Continue to keep ephemeris-data management separate from `SkyCatalogManager`.
`SkyCatalogManager` is scoped to star/deep-sky catalogs and active catalog
rebuilding; `SkyEphemerisDataManager` is the application-layer owner for
high-precision data.

Preferences exposes a dedicated ephemeris page for engine selection,
correction/refraction settings, planetary kernel downloads, support-data
downloads, and cache controls.

Required controls:

- engine selector: `Simple`, `High precision`
- high-precision correction options
- refraction option and atmosphere inputs or preset
- online update state is stored in settings; Preferences currently exposes
  profile downloads, info checks, cancellation, and cache clearing
- kernel status and update action
- EOP/leap-second status and update action
- clear ephemeris data cache
- warning/status text with tooltips

### Settings

Current persistent settings live in
`SkySettingsStore::EphemerisUserSettingsSnapshot`:

- selected engine kind
- high-precision correction flags
- refraction enabled/disabled
- atmosphere defaults for refraction
- preferred kernel/data profile
- online update enabled/disabled
- ephemeris update URLs or preset

Installed data metadata lives in `EphemerisDataCacheSnapshot`:

- installed kernel path and version
- installed EOP path and version
- installed leap-second table version
- installed Delta T data version
- data revision token
- last update result

Keep ephemeris settings and data-cache metadata separate from catalog snapshots.

## Performance Requirements

Primary optimization targets:

- full-frame rendering
- search/filtering across tens of thousands of objects

Secondary targets:

- single tracked object
- trails
- focused object inspection

The engine must avoid recomputing shared per-frame state:

- time-scale conversions for the request epoch
- Earth heliocentric/geocentric state
- observer geocentric position
- Earth-orientation matrices
- precession/nutation matrices
- apparent-place constants
- atmospheric model constants

For catalog stars:

- preprocess immutable catalog astrometry into cache-friendly arrays
- batch proper-motion/parallax calculations where possible
- apply a shared transform pipeline per frame
- avoid per-object heap allocations during full-frame compute

Scene cache keys must include:

- catalog revision
- observer
- astronomical epoch
- engine kind
- engine options revision
- ephemeris data revision
- EOP/leap-second data revision

The engine should be thread-safe for concurrent read-only computations after
construction. Mutable data updates should create a new data set/engine instance
or use a clearly versioned immutable snapshot.

## Build and Dependencies

`vcpkg.json` declares the base and high-precision dependency sets:

- `zlib`
- `calceph`
- `zstd`
- `erfa`

CMake provides:

- `SKYGATE_ENABLE_HIGH_PRECISION_EPHEMERIS`
- CALCEPH, zstd, and ERFA linkage when high precision is enabled
- keep simple engine builds working where high-precision dependencies are
  intentionally disabled
- vcpkg presets so Linux, macOS, and Windows can build the same dependency set

High precision should be enabled for release builds. Developer builds may allow
it to be disabled, but tests for high precision must be present and runnable in
vcpkg-enabled presets.

## Testing Requirements

Add tests under `libs/skygate-ephemeris/tests/engine/` and fixtures under
`libs/skygate-ephemeris/tests/fixtures/ephemeris/`.

Required test groups:

- CALCEPH kernel loading and manifest validation
- UTC/TAI/TT/TDB/UT1 conversions
- leap-second edge cases
- two-part Julian date/civil date conversion, including BCE dates and no year
  zero behavior
- EOP interpolation and fallback behavior
- ICRS/GCRS/CIRS/TIRS/ITRS/topocentric frame transforms
- geometric solar-system vectors against Horizons fixtures
- apparent RA/Dec against Horizons fixtures
- topocentric Moon/Sun/planet positions against Horizons fixtures
- correction flag enable/disable behavior
- atmospheric refraction behavior and disabled behavior
- star proper motion/parallax/radial velocity propagation
- out-of-range degradation warnings
- engine factory fallback/strict creation behavior
- scene cache invalidation on engine option/data revision changes

Reference fixture policy:

- Use compact JSON or CSV fixtures with metadata.
- Include source URL/API parameters, generated date, source frame, time scale,
  target, observer, expected values, and tolerance.
- Store large fixtures and kernel data through Git LFS.
- Keep a small non-LFS smoke fixture so CI can validate basic behavior without
  downloading full kernels.

UI tests must cover:

- Preferences engine selection
- high-precision option persistence
- ephemeris data update controls
- degraded-result warning display/tooltips
- offline fallback behavior

## Migration Plan

### Phase 1: API and Layout

- Complete: simple engine implementation lives under `src/engine/simple/`.
- Complete: engine kind/options/capability/date-range/result-status public
  types exist.
- Complete: `IEphemerisEngine` has request-based APIs and
  `ObservationContext` adapters.
- Complete: `EphemerisEngineFactory` creates simple or high-precision engines
  by request.
- Complete: settings snapshots cover engine selection and high-precision
  options.

### Phase 2: Time and Data Foundations

- Complete: `AstronomicalEpoch` and two-part Julian date support exist.
- Complete: time-scale, leap-second, Delta T, and EOP provider abstractions
  exist.
- Complete: data manifest and active data snapshot plumbing exists.
- Complete: `SkyEphemerisDataManager` owns UI-layer data state.
- Complete: app settings and data revisions participate in scene invalidation.

### Phase 3: CALCEPH Solar-System Engine

- Complete: CALCEPH provider and kernel selection are wired.
- Complete: geometric solar-system body states are implemented.
- Complete: strict and fallback engine creation paths exist.
- Remaining: broaden Horizons geometric vector fixture coverage.

### Phase 4: Apparent and Topocentric Positions

- Complete: frame transformation, correction options, and refraction plumbing
  exist.
- Complete: degraded-result reasons are surfaced through scene/model status.
- Remaining: broaden apparent/topocentric Horizons fixtures and UI tooltips.

### Phase 5: High-Precision Stars and Performance

- Complete: catalog-star astrometry propagation is implemented.
- Complete: computation caching is part of the high-precision facade.
- Remaining: tune batch full-frame paths and add larger performance guards.

### Phase 6: Update UX and Packaging

- In progress: Preferences expose engine and ephemeris-data controls.
- In progress: kernel/EOP/leap-second update flows and cache activation.
- Add bundled compressed data installation to packaging.
- Add checksum validation and atomic activation.
- Validate macOS, Windows, and Linux release packaging.

## Acceptance Criteria

- Users can choose Simple or High precision in Preferences and the choice
  persists across app restarts.
- High precision computes solar-system RA/Dec through CALCEPH-backed kernels.
- High precision supports the requested -13200 to +13200 date range when the
  optional DE441 data set is installed, and shows explicit degradation warnings
  when DE441 is absent or assumptions dominate.
- High precision exposes correction options and callers can request geometric,
  astrometric, or apparent/topocentric outputs.
- Out-of-range or reduced-data cases compute fallback results where possible and
  surface UI warnings with tooltip text.
- Bundled compressed data supports modern-range offline operation on a clean
  install without requiring the multi-gigabyte DE441 data set.
- Preferences can update ephemeris data independently of star/deep-sky catalogs.
- Tests include deterministic reference fixtures and cover time conversion,
  frames, kernels, correction options, warnings, factory behavior, and UI
  persistence.
- Full-frame rendering and search do not recompute shared per-frame
  high-precision state per object.

## Implementation Notes and Risks

- DE441 kernel size is multiple gigabytes. It must be optional and downloadable
  from Preferences rather than included in the default app bundle.
- Modern maximum accuracy may require both DE440 and DE441. If bundle size
  forces a tradeoff, DE440 or DE440s should be bundled for practical modern
  offline use, while DE441 should be installable through the update flow for
  full-range use.
- Ancient apparent positions are limited by Delta T, Earth rotation models,
  historical calendar interpretation, and source catalog quality. Results must
  expose uncertainty/degradation rather than implying modern precision.
- Leap-second representation needs explicit UI and parsing decisions because
  Qt's normal `QTime` path cannot represent `23:59:60`.
- CALCEPH, zstd, and ERFA are available through the checked-in vcpkg manifest
  feature.
- Linux, macOS, and Windows have vcpkg-backed high-precision build paths.
