# Changelog

All notable changes to this project will be documented in this file.

This project follows a lightweight Keep a Changelog style. Versions are tied
to `v*` Git tags and the version declared in `CMakeLists.txt`.

## [Unreleased]

### High-precision ephemeris engine

This release hardens the high-precision ephemeris engine, tightens its
architecture boundaries, and improves diagnostics and test provenance.

- Guarded Delta-T interpolation denominators and rejected duplicate
  effective epochs in table data.
- Fixed UTC interval arithmetic so request epochs are no longer corrupted
  across time scales.
- Removed the simple-only link dependency on high-precision text parsing and
  broke the catalog/engine date-range coupling.
- Fixed the TDB-TT UT1-fraction argument and capped decompressed output and
  network staging to prevent decompression bombs.
- Added real-kernel Horizons acceptance rows at arcsecond/milliarcsecond
  tolerances and relabeled self-derived fixtures.
- Extracted an injected fallback strategy, unified the computation cache, and
  made the factory a complete composition root with lossless request
  overloads.
- Split the time-scale service into focused offset resolvers and the data
  manager into download, cache, and snapshot responsibilities.
- Propagated calceph diagnostics end-to-end and added ephemeris logging.
- Gave geometric corrections a distinct flag and added structured fallback
  provenance.
- Moved core-worthy calendar and Julian-date arithmetic (`CalendarTime`,
  `EpochCodec`, `AstronomicalEpoch`, and related time types) into
  `skygate-core` with dedicated core tests.
- Deduplicated NAIF body IDs, WGS84 geodesy constants, and solar
  mean-anomaly constants into `skygate-core`.
- Consolidated the solar mean-anomaly constant so the shared value feeds
  both the TDB-TT term and the simple Sun position; the simple-only
  approximation changed from 357.53 to 357.528 (a sub-microsecond TDB-TT
  effect; the HP path uses ERFA).
- Removed the dual civil-date/epoch table-entry representation and added
  `AstronomicalEpoch` equality.
- Documented the high-precision interface layer and fallback strategy in the
  architecture notes.

## [1.1.0] - 2026-05-08

### Added

- Search across sky objects, with target selection and object tracking.
- Clickable sky objects with an inspector panel for object details.
- 24-hour trail rendering for selected objects, including pinned inspector
  support.
- Deep-sky object support, including bundled Messier markers and downloadable
  OpenNGC catalog support.
- Additional sky overlays for the ecliptic, celestial equator, circumpolar
  guides, constellation lines, and labels.
- Theme support, including a night-vision theme.
- BCE/BC date entry using proleptic Gregorian years.
- Date/time popup controls and local time-zone display support.
- Moon and twilight/night-condition status.
- Object rise, set, culmination, and angle details.
- Configurable terminal/file logging with log-level controls.
- Linux, macOS, and Windows GitHub Actions CI builds.
- Manual and tag-triggered package workflow for AppImage, DMG, and MSI
  artifacts.
- Release tag validation to keep `v*` tags aligned with the CMake application
  version.
- Package smoke checks for AppImage, DMG, and Windows installer artifacts.
- Non-GUI `--version` output for packaged executable runtime checks.
- 30-day retention policy for manual package workflow artifacts.
- Compiler caches for Linux, macOS, and Windows CI jobs.
- Linux AppImage packaging support.
- Windows WiX MSI packaging support.
- vcpkg-backed zlib dependency management for Windows and macOS packaging.
- Main window size persistence across app launches.
- Extensive Qt, QML, catalog, ephemeris, geometry, property, regression, and
  performance guard tests.

### Changed

- Reworked the sky UI around smaller QML components for search, status,
  preferences, overlays, inspector controls, and interaction layers.
- Refactored the sky scene model into explicit composition, frame pipeline,
  hit-testing, overlay adapter, and render-builder components.
- Split the context controller into focused domain controllers for timeline,
  search/tracking, catalog, location/view, settings, and night conditions.
- Reworked catalog runtime state, cache handling, import workflow, and parser
  boundaries.
- Refactored core projection math into prepared projections, shared projection
  algorithms, spherical geometry helpers, line patterns, and spatial indexing.
- Improved polyline projection to adapt to zoom level.
- Refactored ephemeris and catalog internals into focused calculators,
  factories, loaders, archive readers, and format detectors.
- Reduced status-footer clutter and unified preferences group styling.
- Renamed the settings draft UI to `PreferencesDraft`.
- macOS package builds now produce Apple Silicon DMGs with architecture-aware
  artifact names.
- CI package artifacts now use architecture-aware names on all supported
  platforms.
- Sky viewport drag updates are coalesced to keep packaged builds responsive
  during pan interactions.
- The About window uses a dedicated presentation icon asset.
- Linux CI was optimized for faster core builds.

### Fixed

- Restoring time correctly when reopening from previous live mode.
- Avoiding unnecessary internal index rebuilds when selecting constellations.
- Correctly displaying constellation counts in preferences.
- Toolbar show/hide behavior while animations are running.
- Closing date/time popups from footer interactions.
- QML warnings and rendering-test assumptions around blank frames.
- Windows build compatibility for standard library includes.
- Linux CI issues around linker dependency files, Qt ICU dependencies, ccache
  paths, and LFS-backed fixtures.
- macOS deployment warnings from private SQL plugins.
- Packaged macOS viewport pan performance.
- Windows packages now embed SkyGate icons and include vcpkg runtime DLLs
  required by the installer payload.

### Documentation

- Updated README packaging instructions for AppImage, DMG, and WiX MSI outputs.
- Added a release checklist covering version bumps, changelog updates, tag
  validation, package workflows, and artifact verification.
- Clarified release documentation around the separate Linux, macOS, and Windows
  package workflows.
- Documented the macOS quarantine removal command required for unsigned,
  non-notarized DMG installs.
- Expanded architecture documentation for the current scene, catalog, settings,
  and rendering boundaries.

## [1.0.0] - 2026-04-21

- Initial public release of SkyGate.
- Qt 6 desktop sky viewer with interactive navigation and time controls.
- Reusable core projection and ephemeris libraries.
- Bundled starter catalog, catalog download support, and persistent settings.
