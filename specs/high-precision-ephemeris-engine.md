# High-Precision Ephemeris Engine PRD

Status: draft for implementation planning

Last updated: 2026-05-12

## Purpose

Skygate currently has a simple ephemeris engine that is adequate for coarse
visual placement but not for high-precision astronomy. This document specifies
the product and technical requirements for a new high-precision ephemeris engine
integrated as the single source of truth for sky-object positions.

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

The existing public interface is
`libs/skygate-ephemeris/include/skygate/ephemeris/IEphemerisEngine.hpp`.
It exposes:

- `compute(const core::SkyContext&)`
- `computeBodyState(const core::SkyContext&, std::string_view id)`
- `computeBodyState(const core::SkyContext&, std::size_t index)`

The current simple implementation is a facade over calculator classes in
`libs/skygate-ephemeris/src/engine/*.cpp`, with
`SimpleEphemerisEngine` in `libs/skygate-ephemeris/src/SimpleEphemerisEngine.cpp`.
It supports fixed catalog coordinates and approximate Sun, Moon, and planet
positions. Unsupported bodies return NaN states. Horizontal coordinates are
computed only when observer data is valid.

Known limitations:

- `core::UtcTimePoint` is second-resolution `std::chrono::system_clock`.
- `ITimeSource` only provides `nowUtc()` and cannot expose accuracy, leap
  seconds, UT1, TT/TDB, or clock synchronization status.
- Astronomical time is currently derived directly from UTC epoch seconds.
- GMST, obliquity, Sun, Moon, and planet calculations are approximate.
- There is no supported date-range metadata or per-result degradation status.
- There is no explicit engine kind, options model, or user preference for engine
  selection.
- The scene cache invalidates on catalog revision, observer, UTC time, and
  engine pointer, but not on ephemeris data revision, EOP revision, or correction
  option changes.
- The catalog manager currently rebuilds the active catalog and ephemeris engine
  together, which couples star catalog changes to ephemeris engine selection.

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

The existing `CelestialBodyState` can be extended if source compatibility is
manageable. Otherwise add a richer result type and keep adapters for the current
engine interface during migration.

## Architecture Requirements

### Source Layout

Move engine implementations under dedicated subfolders:

- `libs/skygate-ephemeris/src/engine/simple/`
- `libs/skygate-ephemeris/src/engine/highprecision/`
- shared low-level helpers under `libs/skygate-ephemeris/src/engine/common/`
  only when used by both engines

The public interface remains under
`libs/skygate-ephemeris/include/skygate/ephemeris/`.

### Public API

Add public types under `include/skygate/ephemeris/`:

- `EphemerisEngineKind`
- `EphemerisEngineOptions`
- `EphemerisCorrectionFlags`
- `EphemerisCapabilities`
- `EphemerisDateRange`
- `EphemerisDataSetInfo`
- `EphemerisWarning`
- `EphemerisResultStatus`
- `EphemerisRequest`
- `AstronomicalEpoch`
- `TimeScale`

Extend `IEphemerisEngine` with:

- `kind()`
- `name()`
- `capabilities()`
- `supportedDateRanges()`
- `dataSetInfo()`
- `options()`
- `compute(const EphemerisRequest&)`
- `computeBodyState(const EphemerisRequest&, std::string_view id)`
- `computeBodyState(const EphemerisRequest&, std::size_t index)`

Keep the existing `core::SkyContext` overloads as compatibility adapters during
the migration. They should construct a default apparent/topocentric request
using the engine's configured options.

### Factory

`EphemerisEngineFactory` must become responsible for engine kind selection and
data wiring.

Required creation inputs:

- engine kind
- catalog bodies
- engine options
- kernel/data-set manifest
- time-scale service
- Earth-orientation provider
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
- `FrameTransformer`
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

Bundle data required for offline high-precision operation with practical
installer size:

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
- The download flow must support checksum verification, resumable or restartable
  download behavior, atomic installation, and safe cancellation.

Data assets must be compressed with zstd and decompressed or memory-mapped into
an application data cache before use. Do not put multi-gigabyte kernels into Qt
resource files. Install them as external application resources and copy or
decompress them atomically into the writable app data location on first use.

Use Git LFS for large repository assets:

- reference fixtures
- bundled kernel archives when they must live in the repository
- large generated validation data

### In-App Updates

Add an application-layer `SkyEphemerisDataManager` owned by
`SkyContextController`, parallel to `SkyCatalogManager`.

Responsibilities:

- report bundled and installed ephemeris data status
- update kernels, EOP data, leap seconds, and Delta T data
- verify checksums before activation
- install updates atomically
- preserve offline fallback to bundled data
- emit revision changes that invalidate scene caches

Do not overload `SkyCatalogManager`; it is currently scoped to star/deep-sky
catalogs and active catalog rebuilding.

Preferences should place ephemeris data controls in the existing Catalog page as
a new `Ephemeris Data` group, or rename the page to `Data` if the UI grows.

Required controls:

- engine selector: `Simple`, `High precision`
- high-precision correction options
- refraction option and atmosphere inputs or preset
- offline/online update mode
- kernel status and update action
- EOP/leap-second status and update action
- clear ephemeris data cache
- warning/status text with tooltips

### Settings

Add persistent settings:

- selected engine kind
- high-precision correction flags
- refraction enabled/disabled
- atmosphere defaults for refraction
- preferred kernel/data profile
- online update enabled/disabled
- ephemeris update URLs or preset

Add cache metadata:

- installed kernel path and version
- installed EOP path and version
- installed leap-second table version
- installed Delta T data version
- data revision token
- last update result

Prefer new `EphemerisSettingsSnapshot` and `EphemerisDataCacheSnapshot` types
instead of adding unrelated fields to catalog snapshots.

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

Update `vcpkg.json`:

- `calceph`
- `zstd`
- consider `erfa` through vcpkg overlay, system package, or vendored source if
  no curated vcpkg port is available

Update CMake:

- add `SKYGATE_ENABLE_HIGH_PRECISION_EPHEMERIS`
- find and link CALCEPH when high precision is enabled
- find and link zstd for data archive handling
- keep simple engine builds working where high-precision dependencies are
  intentionally disabled
- add or update vcpkg presets so Linux, macOS, and Windows can build the same
  dependency set

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

- Move simple engine implementation to `src/engine/simple/`.
- Add engine kind/options/capability/date-range/result-status public types.
- Extend `IEphemerisEngine` while preserving current `SkyContext` adapters.
- Update factory to create simple or high-precision engines by request.
- Add settings snapshots for engine selection and high-precision options.

### Phase 2: Time and Data Foundations

- Add `AstronomicalEpoch` and two-part Julian date support.
- Add time-scale, leap-second, Delta T, and EOP provider interfaces.
- Add zstd-compressed data manifest support.
- Add `SkyEphemerisDataManager` and cache metadata.
- Wire app settings and cache revisions into scene invalidation.

### Phase 3: CALCEPH Solar-System Engine

- Add CALCEPH provider and kernel selection.
- Implement geometric solar-system body states.
- Add Horizons geometric vector fixtures.
- Add strict and fallback engine creation paths.

### Phase 4: Apparent and Topocentric Positions

- Add frame transformation pipeline following IAU/IERS conventions.
- Add light-time, aberration, gravitational deflection, parallax, and refraction
  options.
- Add apparent/topocentric Horizons fixtures.
- Surface warnings and provenance in UI inspection/status.

### Phase 5: High-Precision Stars and Performance

- Add catalog-star astrometry propagation with proper motion, parallax, and
  radial velocity.
- Add batch full-frame computation path.
- Tune caches for frame rendering and search.
- Add performance benchmarks or timing tests for large catalogs.

### Phase 6: Update UX and Packaging

- Add Preferences `Ephemeris Data` controls.
- Add kernel/EOP/leap-second update flows.
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
- CALCEPH and zstd are available in vcpkg. ERFA may require an overlay port or
  vendoring if no curated vcpkg port is available.
- The current Linux base build does not appear to use vcpkg. The high-precision
  build path should either add Linux vcpkg presets or keep simple-only builds
  explicit.
