# Changelog

All notable changes to this project will be documented in this file.

This project follows a lightweight Keep a Changelog style. Versions are tied
to `v*` Git tags and the version declared in `CMakeLists.txt`.

## [Unreleased]

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
- Manual and tag-triggered package workflow for AppImage, DMG, MSI, and ZIP
  artifacts.
- Release tag validation to keep `v*` tags aligned with the CMake application
  version.
- Package smoke checks for AppImage, DMG, and Windows installer artifacts.
- Non-GUI `--version` output for packaged executable runtime checks.
- Compiler caches for Linux, macOS, and Windows CI jobs.
- Linux AppImage packaging support.
- Windows WiX MSI packaging support.
- vcpkg-backed zlib dependency management for Windows and macOS packaging.
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

### Documentation

- Updated README packaging instructions for AppImage, DMG, Windows ZIP, and
  WiX MSI outputs.
- Added a release checklist covering version bumps, changelog updates, tag
  validation, package workflows, and artifact verification.
- Expanded architecture documentation for the current scene, catalog, settings,
  and rendering boundaries.

## [1.0.0] - 2026-04-21

- Initial public release of SkyGate.
- Qt 6 desktop sky viewer with interactive navigation and time controls.
- Reusable core projection and ephemeris libraries.
- Bundled starter catalog, catalog download support, and persistent settings.
