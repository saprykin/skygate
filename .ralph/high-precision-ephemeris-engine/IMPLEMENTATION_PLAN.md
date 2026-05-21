# High-Precision Ephemeris Engine Implementation Plan V3

This plan refines `IMPLEMENTATION_PLAN.md` against
`spec/high-precision-ephemeris-engine.md` and the current source tree. It keeps
public API/modeling, data loading/ingestion, numerical computation, validation,
and UI/application wiring as separate work streams.

Status markers:

- `[ ]` not complete
- `[x]` complete

## Scope Guardrails

- Do not remove the current simple engine. It remains selectable and acts as the
  fallback path.
- Do not numerically integrate planetary ephemerides in Skygate. Planetary and
  lunar state must come from JPL Development Ephemerides through CALCEPH.
- Do not put network/update orchestration inside the ephemeris engine. App-layer
  managers own downloads, updates, cache activation, and UI status.
- Do not silently claim high precision outside the validity range of available
  data. Return degraded fallback results where possible and expose warnings.
- Keep star/deep-sky catalog ingestion separate from ephemeris kernel, EOP,
  leap-second, and Delta T data management.
- Keep algorithmic/numerical tasks separate from public API/modeling tasks.

## Phase 1: API, Layout, Build, And Factory

### HP-001: Move the existing simple engine into the required source layout

Status: [x]
Priority: Critical
Type: Layout / migration
Source:
- Spec: Source Layout
- Current code: `libs/skygate-ephemeris/src/SimpleEphemerisEngine.cpp` and
  `libs/skygate-ephemeris/src/engine/*.cpp`

Required work:
- Create `libs/skygate-ephemeris/src/engine/simple/`.
- Move `SimpleEphemerisEngine` and simple-only calculators into that folder.
- Create empty or placeholder build entries for
  `libs/skygate-ephemeris/src/engine/highprecision/`.
- Create `libs/skygate-ephemeris/src/engine/common/` only for helpers actually
  shared by both engines.
- Update `libs/skygate-ephemeris/CMakeLists.txt` without changing simple-engine
  behavior.

Verification:
- Run `clang-format` on moved C++ files.
- Build `skygate-ephemeris`.
- Run existing simple-engine tests:
  `skygate-ephemeris-engine-baseline-tests`,
  `skygate-ephemeris-engine-fallback-tests`, and
  `skygate-ephemeris-regression-tests`.
- Confirm golden simple-engine outputs are unchanged.

### HP-002: Add optional high-precision build dependencies and gates

Status: [x]
Priority: Critical
Type: Build
Source:
- Spec: Build and Dependencies
- Current code: `vcpkg.json`, `CMakePresets.json`,
  `libs/skygate-ephemeris/CMakeLists.txt`

Required work:
- Add `SKYGATE_ENABLE_HIGH_PRECISION_EPHEMERIS` as a CMake option.
- Add `calceph` and `zstd` to vcpkg dependency configuration.
- Keep zlib catalog support unchanged.
- When high precision is disabled, configure and build without requiring
  CALCEPH, zstd, or ERFA.
- When high precision is enabled, require CALCEPH and zstd and link them only to
  targets that need them.
- Add or update vcpkg-enabled presets for supported platforms so high-precision
  tests can be configured consistently.

Verification:
- Configure/build with `SKYGATE_ENABLE_HIGH_PRECISION_EPHEMERIS=OFF`; verify
  CALCEPH is not required.
- Configure/build with `SKYGATE_ENABLE_HIGH_PRECISION_EPHEMERIS=ON`; verify
  CALCEPH and zstd are found and linked, or configuration fails with a clear
  dependency error.
- Add a dependency smoke test that is compiled/runnable only when high precision
  is enabled.

### HP-003: Choose and wire the ERFA/SOFA strategy

Status: [x]
Priority: Critical
Type: Build / numerical dependency
Source:
- Spec: Field Research Summary; Build and Dependencies

Required work:
- Decide the ERFA integration route: vcpkg overlay, system package, or vendored
  source.
- Wire the chosen route into CMake behind
  `SKYGATE_ENABLE_HIGH_PRECISION_EPHEMERIS`.
- Keep the public code calling an internal wrapper rather than spreading ERFA
  symbols across the codebase.
- Document the chosen dependency path in build comments or developer docs where
  the CMake option is defined.

Verification:
- Add a high-precision-only smoke test for one known ERFA/SOFA time or frame
  routine.
- Verify simple-only builds compile with no ERFA dependency.

### HP-004: Add public high-precision API model types

Status: [x]
Priority: Critical
Type: Public API / modeling
Source:
- Spec: Public API; Correction Options; Accuracy and Ranges
- Current code: `libs/skygate-ephemeris/include/skygate/ephemeris/Types.hpp`

Required work:
- Add public types under `include/skygate/ephemeris/`:
  `EphemerisEngineKind`, `EphemerisEngineOptions`,
  `EphemerisCorrectionFlags`, `EphemerisCapabilities`,
  `EphemerisDateRange`, `EphemerisDataSetInfo`, `EphemerisRequest`,
  `AstronomicalEpoch`, and `TimeScale`.
- Represent exactly two top-level engine kinds: `Simple` and `HighPrecision`.
- Make correction flags explicit enough to request geometric, astrometric, and
  apparent/topocentric outputs.
- Include option fields for refraction and atmosphere inputs required by the
  spec.
- Keep current `CelestialBody`, `CelestialBodyState`, and `SkySnapshot`
  source-compatible where practical.

Verification:
- Add compile/API tests that instantiate defaults, combine correction flags, and
  validate exactly two engine kinds.
- Verify existing simple-engine clients still compile.

### HP-049: Restore missing public high-precision API model types

Status: [x]
Priority: Critical
Type: Public API / modeling follow-up
Source:
- Completed task: HP-004
- Current code: `libs/skygate-ephemeris/include/skygate/ephemeris/Types.hpp`

Required work:
- Audit `Types.hpp` against HP-004 and the high-precision spec.
- Add any HP-004 public model types that are still missing from `Types.hpp`,
  including the high-precision request, options, capabilities, date range, data
  set, epoch, time scale, and correction flag models.
- Keep this follow-up separate from HP-002; it tracks public API/modeling
  completeness, not optional build dependency gates.
- Preserve source compatibility for existing simple-engine clients where
  practical.

Verification:
- Add or update compile/API tests that instantiate the restored model types,
  combine correction flags, and validate the public engine-kind model.
- Verify existing simple-engine clients still compile.

### HP-050: Rename catalog public `None` enumerators to avoid macro collisions

Status: [x]
Priority: Critical
Type: Public API / catalog follow-up
Source:
- Review finding from HP-049 fix: unrelated public-header macro hazard in
  catalog APIs.
- Current code: public catalog headers defining `CatalogSelectionMode::None`
  and `CatalogLoadErrorCode::None`.

Required work:
- Audit public catalog headers for enumerators or APIs exposed in public
  include order that can collide with platform macros such as X11 `None`.
- Rename `CatalogSelectionMode::None` and `CatalogLoadErrorCode::None` to
  collision-resistant names while preserving source compatibility where
  practical.
- Add compile/API coverage for X11-style `None` macro include order so public
  catalog headers remain usable when platform headers define `None`.

Verification:
- Run `clang-format` on any touched C++ source or header files.
- Run relevant catalog/API tests, including the new include-order coverage.

### HP-051: Fix footer popup toolbar toggle QML regression

Status: [x]
Priority: High
Type: UI / test follow-up
Source:
- HP-050 verification finding: unrelated persistent UI test failure after
  building all targets in `build-ralph` with UI enabled.
- Failing test: `skygate-ui-qml-main-window-tests`,
  `QmlMainWindowTests::footerPopupToolbarToggleClickClosesPopupAndTogglesToolbar()`
  at `apps/skygate-ui/tests/qml/QmlMainWindowTests.cpp(219)`, where
  `!controller->timelineToolbarCollapsed()` returned false.

Required work:
- Reproduce the failing QML main-window test in a UI-enabled `build-ralph`
  build.
- Investigate the footer popup close/timeline toolbar toggle interaction and
  determine whether the regression is in the QML event path, controller state
  synchronization, or the test expectation.
- Fix the UI behavior or test harness so the footer popup toolbar toggle click
  both closes the popup and leaves `timelineToolbarCollapsed()` in the expected
  state.
- Keep this follow-up separate from HP-050; it appears unrelated to public
  catalog enum macro compatibility.

Verification:
- Build all targets in `build-ralph` with UI enabled.
- Run `ctest --test-dir build-ralph --output-on-failure` and verify all 103
  tests pass.
- Run `skygate-ui-qml-main-window-tests` directly if needed to confirm
  `footerPopupToolbarToggleClickClosesPopupAndTogglesToolbar()` is stable.

Notes:
- HP-050 verification observed 102/103 tests passing with only
  `skygate-ui-qml-main-window-tests` failing persistently.

### HP-052: Re-fix footer popup toolbar toggle QML regression

Status: [x]
Priority: High
Type: UI / test follow-up
Source:
- HP-008A verification finding: unrelated persistent full-suite UI test failure
  after building all targets in `build-ralph` with UI enabled.
- Failing test: `skygate-ui-qml-main-window-tests`,
  `QmlMainWindowTests::footerPopupToolbarToggleClickClosesPopupAndTogglesToolbar()`
  at `apps/skygate-ui/tests/qml/QmlMainWindowTests.cpp(219)`, where
  `!controller->timelineToolbarCollapsed()` returned false.

Required work:
- Reproduce the failing QML main-window test in the current UI-enabled
  `build-ralph` build.
- Re-investigate the footer popup close/timeline toolbar toggle interaction;
  HP-051 was marked complete, but the same failure is present again.
- Fix the UI behavior or test harness so the footer popup toolbar toggle click
  both closes the popup and leaves `timelineToolbarCollapsed()` in the expected
  state.
- Keep this follow-up separate from HP-008A; it is unrelated to the factory
  request/fallback-policy API model.

Verification:
- Build all targets in `build-ralph` with UI enabled.
- Run `ctest --test-dir build-ralph --output-on-failure` and verify all tests
  pass.
- Run `skygate-ui-qml-main-window-tests` directly if needed to confirm
  `footerPopupToolbarToggleClickClosesPopupAndTogglesToolbar()` is stable.

Notes:
- HP-008A verification on 2026-05-13 observed 103/104 tests passing with only
  `skygate-ui-qml-main-window-tests` failing persistently.

### HP-053: Fix recurring QML main-window verification failures

Status: [x]
Priority: High
Type: UI / test follow-up
Source:
- HP-009 verification finding: unrelated full-suite UI failures after building
  all targets in `build-ralph`.
- Failing test: `skygate-ui-qml-main-window-tests`,
  `QmlMainWindowTests::footerPopupToolbarToggleClickClosesPopupAndTogglesToolbar()`
  at `apps/skygate-ui/tests/qml/QmlMainWindowTests.cpp(219)`, where
  `!controller->timelineToolbarCollapsed()` returned false.
- Failing test: `skygate-ui-qml-main-window-rendering-tests`,
  `QmlMainWindowRenderingTests::mainWindowsRenderNonBlankAndKeepVisibleControlsInBounds()`
  at `apps/skygate-ui/tests/qml/QmlMainWindowRenderingTests.cpp(58)`, where
  `SkyOverlayLabel_QMLTYPE_84` was reported outside the `1100x760` viewport.

Required work:
- Reproduce both failing QML tests in the current UI-enabled `build-ralph`
  build.
- Re-investigate the footer popup close/timeline toolbar toggle interaction;
  HP-052 was marked complete, but the same failure is present again.
- Investigate why sky overlay labels can be positioned above the visible main
  window bounds in the rendering coverage.
- Fix the QML behavior or test harness so footer popup toggling is stable and
  visible overlay controls remain within the tested viewport.
- Keep this follow-up separate from HP-009; it is unrelated to the
  high-precision engine facade boundary.

Verification:
- Build all targets in `build-ralph` with UI enabled.
- Run `ctest --test-dir build-ralph --output-on-failure` and verify all tests
  pass.
- Run `skygate-ui-qml-main-window-tests` and
  `skygate-ui-qml-main-window-rendering-tests` directly if needed to confirm
  both failures are stable.

Notes:
- HP-009 verification on 2026-05-13 observed 105/107 tests passing with only
  `skygate-ui-qml-main-window-tests` and
  `skygate-ui-qml-main-window-rendering-tests` reporting failures.

### HP-054: Re-verify footer popup toolbar toggle QML failure

Status: [x]
Priority: High
Type: UI / test follow-up
Source:
- HP-020 verification finding: unrelated full-suite UI failure after building
  all targets in `build-ralph`.
- Failing test: `skygate-ui-qml-main-window-tests`,
  `QmlMainWindowTests::footerPopupToolbarToggleClickClosesPopupAndTogglesToolbar()`
  at `apps/skygate-ui/tests/qml/QmlMainWindowTests.cpp(219)`, where
  `!controller->timelineToolbarCollapsed()` returned false.

Required work:
- Reproduce the current UI-enabled `build-ralph` failure.
- Investigate footer popup close/timeline toolbar toggle state synchronization.
- Fix the QML behavior or test harness so the footer popup toolbar toggle click
  both closes the popup and leaves `timelineToolbarCollapsed()` in the expected
  state.
- Keep this follow-up separate from HP-020; it is unrelated to that work.

Verification:
- Build all targets in `build-ralph` with UI enabled.
- Run `ctest --test-dir build-ralph --output-on-failure` and verify all tests
  pass.
- Run `skygate-ui-qml-main-window-tests` directly to confirm
  `footerPopupToolbarToggleClickClosesPopupAndTogglesToolbar()` is stable.

Notes:
- HP-020 verification on 2026-05-13 observed 112/113 tests passing with only
  `skygate-ui-qml-main-window-tests` failing.

### HP-055: Fix recurring footer popup toolbar toggle QML failure

Status: [x]
Priority: High
Type: UI / test follow-up
Source:
- HP-023 verification finding: unrelated full-suite UI failure after building
  all targets in `build-ralph`.
- Failing test: `skygate-ui-qml-main-window-tests`,
  `QmlMainWindowTests::footerPopupToolbarToggleClickClosesPopupAndTogglesToolbar()`
  at `apps/skygate-ui/tests/qml/QmlMainWindowTests.cpp(219)`, where
  `!controller->timelineToolbarCollapsed()` returned false.

Required work:
- Reproduce the failure in the current UI-enabled `build-ralph` build.
- Investigate footer popup toolbar toggle state synchronization, event path,
  and test harness behavior.
- Fix the UI behavior or test harness so the footer popup toolbar toggle click
  both closes the popup and leaves `timelineToolbarCollapsed()` in the expected
  state.
- Keep this follow-up separate from HP-023; it is unrelated to that work.

Verification:
- Build all targets in `build-ralph` with UI enabled.
- Run `ctest --test-dir build-ralph --output-on-failure` and verify all tests
  pass.
- Run `skygate-ui-qml-main-window-tests` directly if needed to confirm
  `footerPopupToolbarToggleClickClosesPopupAndTogglesToolbar()` is stable.

Notes:
- HP-023 verification on 2026-05-13 observed 114/115 tests passing with only
  `skygate-ui-qml-main-window-tests` failing.

### HP-056: Reproduce and fix footer popup toolbar toggle state synchronization

Status: [x]
Priority: High
Type: UI / test follow-up
Source:
- HP-025 verification finding: unrelated full-suite UI failure after building
  all targets in `build-ralph`.
- Full `ctest` on 2026-05-13 in `build-ralph` passed 115/116 tests, with only
  `skygate-ui-qml-main-window-tests` failing.
- Failing test:
  `QmlMainWindowTests::footerPopupToolbarToggleClickClosesPopupAndTogglesToolbar()`
  at `apps/skygate-ui/tests/qml/QmlMainWindowTests.cpp(219)`, where
  `!controller->timelineToolbarCollapsed()` returned false.

Required work:
- Reproduce the footer popup close/timeline toolbar toggle failure in the
  current UI-enabled `build-ralph` build.
- Investigate footer popup close handling and timeline toolbar toggle state
  synchronization across the QML event path, controller state, and test
  harness.
- Fix the UI behavior or test harness so the footer popup toolbar toggle click
  closes the popup and leaves `timelineToolbarCollapsed()` in the expected
  state.
- Keep this follow-up separate from HP-025; it is unrelated to geometric
  solar-system vector calculation.

Verification:
- Build all targets in `build-ralph` with UI enabled.
- Run `ctest --test-dir build-ralph --output-on-failure` and verify all tests
  pass.
- Run `skygate-ui-qml-main-window-tests` directly to confirm
  `footerPopupToolbarToggleClickClosesPopupAndTogglesToolbar()` is stable.

### HP-057: Re-check footer popup toolbar toggle QML failure

Status: [x]
Priority: High
Type: UI / test follow-up
Source:
- HP-040 verification finding: unrelated full-suite failure observed during
  verification.
- Failing test: `skygate-ui-qml-main-window-tests`,
  `QmlMainWindowTests::footerPopupToolbarToggleClickClosesPopupAndTogglesToolbar()`
  at `apps/skygate-ui/tests/qml/QmlMainWindowTests.cpp(219)`, where
  `!controller->timelineToolbarCollapsed()` returned false.

Required work:
- Reproduce the footer popup close/timeline toolbar toggle failure in the
  current UI-enabled build.
- Investigate footer popup close handling and timeline toolbar toggle state
  synchronization across the QML event path, controller state, and test
  harness.
- Fix the UI behavior or test harness so the footer popup toolbar toggle click
  closes the popup and leaves `timelineToolbarCollapsed()` in the expected
  state.
- Keep this follow-up separate from HP-040; it is unrelated to engine ownership
  and catalog rebuild behavior.

Verification:
- Build all targets with UI enabled.
- Run `ctest --output-on-failure` and verify all tests pass.
- Run `skygate-ui-qml-main-window-tests` directly to confirm
  `footerPopupToolbarToggleClickClosesPopupAndTogglesToolbar()` is stable.

### HP-058: Reproduce build-ralph footer popup toolbar toggle QML failure

Status: [x]
Priority: High
Type: UI / test follow-up
Source:
- HP-022C verification finding: unrelated QML failure observed during
  verification.
- Failing test: `skygate-ui-qml-main-window-tests`,
  `QmlMainWindowTests::footerPopupToolbarToggleClickClosesPopupAndTogglesToolbar()`
  at `apps/skygate-ui/tests/qml/QmlMainWindowTests.cpp(219)`, where
  `!controller->timelineToolbarCollapsed()` returned false.

Required work:
- Reproduce the current `build-ralph` failure.
- Investigate footer popup close/timeline toolbar toggle synchronization across
  the QML event path, controller state, and test harness.
- Fix the UI behavior or test harness so the footer popup toolbar toggle click
  closes the popup and leaves `timelineToolbarCollapsed()` in the expected
  state.
- Keep this follow-up separate from HP-022C; it is unrelated to staged update
  set verification.

Verification:
- Build all targets in `build-ralph`.
- Run `ctest --test-dir build-ralph --output-on-failure` and verify all tests
  pass.
- Run `skygate-ui-qml-main-window-tests` directly if needed to confirm
  `footerPopupToolbarToggleClickClosesPopupAndTogglesToolbar()` is stable.

Notes:
- HP-022C full-suite verification on 2026-05-14 observed 119/120 tests passing
  with only this QML test failing.

### HP-059: Keep main-window overlay labels inside the viewport

Status: [x]
Priority: High
Type: UI / test follow-up
Source:
- HP-029 verification finding: unrelated full-suite failure observed during
  verification.
- Failing test: `skygate-ui-qml-main-window-rendering-tests`,
  `QmlMainWindowRenderingTests::mainWindowsRenderNonBlankAndKeepVisibleControlsInBounds()`
  at `apps/skygate-ui/tests/qml/QmlMainWindowRenderingTests.cpp(58)`, where
  `SkyOverlayLabel_QMLTYPE_83` was at `[444.744,-15.9039 53.6719x19]`
  outside the `1100x760` viewport.

Required work:
- Reproduce the `build-ralph` failure from the full-suite run.
- Investigate overlay label positioning and bounds checks in the main-window
  rendering coverage.
- Fix QML behavior or the test harness so visible overlay controls remain
  within the viewport.
- Keep this follow-up separate from HP-029; it is unrelated to atmospheric
  refraction calculation.

Verification:
- Build all targets in `build-ralph`.
- Run `ctest --test-dir build-ralph --output-on-failure` and verify all tests
  pass.
- Run `skygate-ui-qml-main-window-rendering-tests` directly if needed to
  confirm
  `mainWindowsRenderNonBlankAndKeepVisibleControlsInBounds()` is stable.

Notes:
- HP-029 full-suite verification on 2026-05-14 observed 120/121 tests passing
  with only this QML rendering test failing.

### HP-005: Add public result status and provenance model

Status: [x]
Priority: Critical
Type: Public API / modeling
Source:
- Spec: Result Status; Accuracy and Ranges
- Current code: `CelestialBodyState` currently has only body index,
  equatorial, and horizontal coordinates.

Required work:
- Add `EphemerisResultStatus` with at least valid, degraded, unsupported,
  out-of-range, and failed states.
- Add `EphemerisWarning` with stable warning codes and non-empty display text.
- Add result metadata for data source provenance, effective data validity range,
  estimated angular uncertainty when available, and correction flags actually
  applied.
- Decide whether metadata extends `CelestialBodyState` directly or lives in a
  richer result type with adapters for the current state shape.
- Preserve compatibility adapters for existing `SkySnapshot` consumers during
  migration.

Verification:
- Add unit tests for all status values and representative warning codes.
- Test that warning text is non-empty for degraded, unsupported, out-of-range,
  and failed results.
- Test that simple-engine compatibility states remain readable by existing
  scene/search/trail code.

### HP-006: Extend `IEphemerisEngine` with request-based methods

Status: [x]
Priority: Critical
Type: Public API / interface umbrella
Source:
- Spec: Public API
- Current code:
  `libs/skygate-ephemeris/include/skygate/ephemeris/IEphemerisEngine.hpp`

Required work:
- Keep this as the parent task for the interface migration.
- Mark complete only when HP-006A through HP-006E are complete.
- Preserve the dependency chain from HP-004 and HP-005 into HP-008 and HP-009.

Verification:
- All HP-006 child verification items pass.

#### HP-006A: Add engine metadata accessors to `IEphemerisEngine`

Status: [x]
Priority: Critical
Type: Public API / interface
Dependencies:
- HP-004
- HP-005

Required work:
- Add `kind()`, `name()`, `capabilities()`, `supportedDateRanges()`,
  `dataSetInfo()`, and `options()` to `IEphemerisEngine`.
- Provide simple-engine metadata defaults that reflect existing behavior.
- Keep the metadata model source-compatible with existing simple-engine clients
  where practical.

Verification:
- Add API tests for simple-engine metadata defaults.
- Verify existing simple-engine clients still compile after the accessor
  additions.

#### HP-006B: Add request-based snapshot compute API

Status: [x]
Priority: Critical
Type: Public API / interface
Dependencies:
- HP-004
- HP-005
- HP-006A

Required work:
- Add `compute(const EphemerisRequest&)` to `IEphemerisEngine`.
- Implement the request path for the simple engine using current simple-engine
  behavior and the request's epoch, observer, and option fields.
- Preserve the existing `SkySnapshot` shape during the migration.

Verification:
- Add API tests proving `EphemerisRequest` snapshot compute works.
- Verify request fields used by the simple-engine adapter produce the same
  coordinates as the current `core::SkyContext` path for equivalent inputs.

#### HP-006C: Add request-based body lookup overloads

Status: [x]
Priority: Critical
Type: Public API / interface
Dependencies:
- HP-006B

Required work:
- Add `computeBodyState(const EphemerisRequest&, std::string_view id)`.
- Add `computeBodyState(const EphemerisRequest&, std::size_t index)`.
- Preserve current body-id and body-index lookup semantics.

Verification:
- Add API tests for request-based body lookup by id and by index.
- Add missing-body tests for both overloads.

#### HP-006D: Convert `SkyContext` methods into compatibility adapters

Status: [x]
Priority: Critical
Type: Public API / compatibility
Dependencies:
- HP-006B
- HP-006C

Required work:
- Keep the existing `core::SkyContext` overloads callable.
- Make the `core::SkyContext` overloads construct a default
  apparent/topocentric `EphemerisRequest` using the engine's configured
  options.
- Keep compatibility adapters available for existing rendering, search, trail,
  and event consumers until those consumers migrate to explicit requests.

Verification:
- Add API tests proving both `core::SkyContext` and `EphemerisRequest` paths
  work.
- Add tests that the `core::SkyContext` adapters apply engine default options.

#### HP-006E: Update fake/test engines and interface migration tests

Status: [x]
Priority: Critical
Type: Testing / compatibility
Dependencies:
- HP-006D

Required work:
- Update fake/test engines across ephemeris and UI tests to implement the new
  interface.
- Add focused tests for metadata defaults, request compute, request body lookup,
  `core::SkyContext` compatibility, and default option application.

Verification:
- Existing simple-engine tests compile and pass after fake/test engine updates.
- New API tests cover the interface migration surface.

### HP-007: Add catalog astrometry fields and metadata

Status: [x]
Priority: High
Type: Public API / catalog modeling
Source:
- Spec: Correction Options; Accuracy and Ranges
- Current code: catalog stars are fixed equatorial coordinates.

Required work:
- Add a public catalog-star astrometry representation that can carry proper
  motion, radial velocity, stellar parallax, reference epoch, and catalog
  validity metadata when source data provides it.
- Keep fixed-equatorial catalog bodies supported for simple and imported data.
- Update catalog composition so built-in/fixed bodies remain valid even when no
  astrometry metadata exists.

Verification:
- Add catalog model tests for fixed-only stars, stars with full astrometry, and
  stars with partial astrometry.
- Verify catalog parsing/composition tests still pass for existing fixtures.

### HP-008: Rework ephemeris factory request/result API

Status: [x]
Priority: Critical
Type: Public API / factory umbrella
Source:
- Spec: Factory
- Current code:
  `libs/skygate-ephemeris/include/skygate/ephemeris/EphemerisEngineFactory.hpp`

Required work:
- Keep this as the parent task for factory API and creation-policy migration.
- Mark complete only when HP-008A through HP-008F are complete.
- Keep current simple-engine creation overloads available throughout the
  migration.

Verification:
- All HP-008 child verification items pass.

#### HP-008A: Define factory request and fallback policy model

Status: [x]
Priority: Critical
Type: Public API / factory
Dependencies:
- HP-004
- HP-005
- HP-006

Required work:
- Add a factory request type containing engine kind, catalog bodies, engine
  options, kernel/data-set manifest or active data snapshot, time-scale service,
  Earth-orientation provider, fallback policy, and optional diagnostics sink.
- Define fallback policy values that distinguish strict high precision from
  explicitly allowed simple-engine fallback.
- Keep this API-only; use forward declarations or opaque snapshot/provider
  handles where later time, EOP, and data-manager types are not available yet.

Verification:
- Add compile/API tests that construct simple and high-precision factory
  requests.
- Add tests for fallback policy defaults and explicit strict/fallback modes.

#### HP-008B: Define structured factory result and creation errors

Status: [x]
Priority: Critical
Type: Public API / factory
Dependencies:
- HP-005
- HP-006

Required work:
- Add a factory result type containing either an engine or structured creation
  errors.
- Add stable creation error/status codes and non-empty diagnostic text.
- Include helper queries such as `isSuccess()` where useful.
- Represent strict high-precision failure separately from explicit fallback to
  the simple engine.

Verification:
- Add tests for success, strict failure, fallback success with diagnostics, and
  non-empty error text.

#### HP-008C: Preserve simple-engine compatibility overloads

Status: [x]
Priority: Critical
Type: Public API / compatibility
Dependencies:
- HP-001
- HP-006
- HP-008A
- HP-008B

Required work:
- Keep existing `createEphemerisEngine(...)` overloads as simple-engine
  compatibility helpers.
- Route compatibility overloads through the new request/result implementation
  internally where practical.
- Preserve current simple-engine behavior for callers that do not pass a factory
  request.

Verification:
- Add tests for existing no-argument, catalog, and body-span creation overloads.
- Confirm golden simple-engine outputs remain unchanged.

#### HP-008D: Implement factory selection for simple and unavailable high precision

Status: [x]
Priority: Critical
Type: Public API / factory
Dependencies:
- HP-002
- HP-003
- HP-004
- HP-008A
- HP-008B
- HP-008C

Required work:
- Create a simple engine when the request selects `Simple`.
- When `HighPrecision` is requested but build dependencies or data are
  unavailable, return the simple engine only when fallback is explicitly
  allowed.
- When strict high precision is requested and cannot be created, return a
  structured error and no engine.
- Do not construct the real high-precision engine in this subtask.

Verification:
- Add tests for simple creation, high-precision unavailable fallback, strict
  high-precision failure, option propagation, and diagnostics/error text.

#### HP-008E: Add factory request/result behavior tests

Status: [x]
Priority: Critical
Type: Testing / factory
Dependencies:
- HP-008A
- HP-008B
- HP-008C
- HP-008D

Required work:
- Add focused factory tests for request construction, result helpers,
  compatibility overloads, fallback policy, strict failure, and diagnostics.
- Keep real data-set and provider propagation tests limited to opaque or fake
  objects at this phase.

Verification:
- Factory tests cover all request/result code paths available before real
  high-precision construction is wired.

#### HP-008F: Wire real high-precision construction once dependencies exist

Status: [x]
Priority: Critical
Type: Factory / high-precision wiring
Dependencies:
- HP-009
- HP-011
- HP-015
- HP-018
- HP-021
- HP-023
- HP-008D

Required work:
- Construct `HighPrecisionEphemerisEngine` from the factory request using the
  active data snapshot, time-scale service, Earth-orientation provider,
  diagnostics sink, and engine options.
- Propagate data-set and provider metadata into the created engine.
- Preserve strict and fallback behavior when real high-precision construction
  fails.

Verification:
- Add factory tests with fake or smoke high-precision dependencies proving
  real-construction wiring, data-set propagation, provider propagation, and
  strict/fallback diagnostics.

### HP-009: Add the high-precision engine facade boundary

Status: [x]
Priority: Critical
Type: Engine architecture
Source:
- Spec: High-Precision Engine Facade

Required work:
- Add `HighPrecisionEphemerisEngine` under
  `src/engine/highprecision/`.
- Keep it as a facade over focused providers/calculators:
  `CalcephKernelProvider`, `SolarSystemStateCalculator`,
  `StarAstrometryCalculator`, `TimeScaleService`, `EarthOrientationProvider`,
  `FrameTransformer`, `ApparentPlaceCalculator`,
  `AtmosphericRefractionCalculator`, `EphemerisResultBuilder`, and
  `EphemerisComputationCache`.
- The facade validates requests, coordinates dependencies, and assembles
  results/status/provenance.
- Do not implement numerical algorithms in this task; use fake/stub
  collaborators where needed.

Verification:
- Add facade tests with fake collaborators proving request validation,
  collaborator dispatch by body type, option forwarding, and result assembly.
- Verify unsupported bodies return structured unsupported status, not silent
  NaNs, on the high-precision path.

## Phase 2: Time Model And Time Data

### HP-010: Add astronomical time primitives

Status: [x]
Priority: Critical
Type: Public API / time modeling
Source:
- Spec: Time Model
- Current code:
  `libs/skygate-ephemeris/src/engine/AstronomicalTime.*` is a simple internal
  UTC-seconds helper.

Required work:
- Add `AstronomicalEpoch` as a two-part Julian date plus `TimeScale`.
- Add `CivilDateTime` with signed astronomical year, month, day, time, and
  subsecond fields.
- Define no-year-zero behavior explicitly in conversion helpers.
- Keep `core::UtcTimePoint` and `ITimeSource` as current-time interfaces only;
  do not turn them into astronomy services.

Verification:
- Add tests for two-part Julian date construction, normalization, precision
  preservation, BCE dates, and no-year-zero conversion behavior.
- Verify existing `ITimeSource` tests remain unchanged.

### HP-011: Add leap-second table loading

Status: [x]
Priority: Critical
Type: Data loading / time data
Source:
- Spec: Time Model; Data Management; Testing Requirements

Required work:
- Add a `LeapSecondProvider` interface.
- Add a table-backed implementation that loads bundled or installed
  leap-second data from the ephemeris data snapshot.
- Expose the table version and validity range for result provenance and date
  range reporting.
- Report missing/stale table status without performing time-scale conversion
  policy in the loader.

Verification:
- Add loader tests for valid table, malformed table, missing table, stale table,
  and validity-range metadata.

### HP-012: Implement UTC, TAI, and TT conversion

Status: [x]
Priority: Critical
Type: Numerical / time conversion
Source:
- Spec: Time Model; Testing Requirements

Required work:
- Implement UTC to TAI and TT conversion in `TimeScaleService` using
  `LeapSecondProvider`.
- Support leap-second instants such as `23:59:60` through `CivilDateTime`.
- Return degraded or failed conversion status when the request is outside the
  leap-second table range and no permitted fallback applies.

Verification:
- Add tests for normal UTC dates, leap-second boundaries, `23:59:60`, table
  range boundaries, and missing-table degraded status.

### HP-013: Add Delta T data/model provider

Status: [x]
Priority: Critical
Type: Data loading / time data
Source:
- Spec: Time Model; Data Management

Required work:
- Add a `DeltaTProvider` interface.
- Add support for bundled or installed Delta T data/model metadata.
- Expose version, source, validity range, and degradation state.
- Keep use of Delta T in time conversion separate from this loading/model
  provider task.

Verification:
- Add provider tests for present data, missing data, ancient-date fallback
  metadata, and validity-range reporting.

### HP-014: Implement TT, TDB, and UT1 conversion policy

Status: [x]
Priority: Critical
Type: Numerical / time conversion umbrella
Source:
- Spec: Time Model; Testing Requirements

Required work:
- Keep this as the parent task for post-TT time-scale conversion policy.
- Mark complete only when HP-014A and HP-014B are complete.
- Preserve subsecond precision through every conversion path.

Verification:
- All HP-014 child verification items pass.

#### HP-014A: Implement TT and TDB conversion policy

Status: [x]
Priority: Critical
Type: Numerical / time conversion
Dependencies:
- HP-003
- HP-010
- HP-012

Required work:
- Extend `TimeScaleService` to convert between TT and TDB through the ERFA/SOFA
  wrapper or a contained equivalent.
- Preserve two-part Julian date precision through TT/TDB conversion.
- Return status/warnings for any approximation or unavailable inputs required
  by the chosen TT/TDB method.

Verification:
- Add deterministic TT/TDB conversion tests against ERFA/SOFA reference values
  or checked fixtures.
- Add precision-preservation tests across subsecond inputs.

#### HP-014B: Implement UT1 conversion policy

Status: [x]
Priority: Critical
Type: Numerical / time conversion
Dependencies:
- HP-012
- HP-013
- HP-015
- HP-016

Required work:
- Extend `TimeScaleService` to convert to and from UT1 using interpolated
  Earth-orientation inputs when available.
- Define Delta T fallback behavior for ancient, future, missing, or stale EOP
  intervals where policy permits an estimated UT1.
- Return status/warnings when conversion relies on historical assumptions,
  future predictions, stale EOP, missing EOP, or Delta T fallback.
- Preserve subsecond precision through UT1 conversion.

Verification:
- Add deterministic UT1 conversion tests for exact EOP samples, interpolated
  samples, and range boundaries.
- Add tests for ancient-date degraded conversion, future-date prediction
  warnings, stale EOP warnings, missing EOP fallback, and disallowed fallback
  failure.

### HP-015: Add Earth-orientation data loading

Status: [x]
Priority: Critical
Type: Data loading / EOP data
Source:
- Spec: Time Model; Data Management; Testing Requirements

Required work:
- Add an `EarthOrientationProvider` data interface for UT1-UTC and polar motion.
- Add a table-backed loader for bundled or installed EOP data.
- Expose version, source, prediction interval, validity range, and missing/stale
  data status.
- Keep interpolation and transform use out of this loader task.

Verification:
- Add loader tests for valid EOP data, malformed rows, missing values,
  prediction-interval metadata, and stale/missing data warnings.

### HP-016: Implement EOP interpolation and fallback behavior

Status: [x]
Priority: Critical
Type: Numerical / Earth orientation
Source:
- Spec: Time Model; Testing Requirements

Required work:
- Interpolate UT1-UTC and polar motion values from `EarthOrientationProvider`.
- Define fallback behavior when the requested epoch is outside EOP coverage.
- Return warning/status metadata for stale, predicted, or missing EOP data.

Verification:
- Add interpolation tests at exact samples, between samples, at range edges, and
  outside range.
- Add tests that stale/missing EOP data changes result status to degraded rather
  than silently applying high-precision claims.

### HP-017: Convert UI date/time inputs to astronomical epochs

Status: [x]
Priority: High
Type: App integration / time modeling
Source:
- Spec: Time Model; Implementation Notes and Risks
- Current code: UI and settings store UTC seconds through `QDateTime`.

Required work:
- Add controller-side conversion from UI date/time text to `CivilDateTime` and
  `AstronomicalEpoch` before high-precision requests.
- Decide and implement UI parsing behavior for leap-second text.
- Preserve current `QDateTime`/`UtcTimePoint` flow for simple-engine current
  dates.
- Do not extend `ITimeSource`; live time still uses the current clock
  abstraction.

Verification:
- Add controller/text-codec tests for valid modern UTC, leap-second input,
  BCE dates, no-year-zero rejection or mapping, malformed text, and simple-engine
  compatibility.

## Phase 3: Ephemeris Data Loading, Cache, And Updates

### HP-018: Define ephemeris data manifest schema and parser

Status: [x]
Priority: Critical
Type: Data loading / manifest
Source:
- Spec: Data Management

Required work:
- Define a manifest format for kernels, leap-second data, EOP data, Delta T
  data, source URLs, versions, checksums, validity ranges, compression metadata,
  and data-set profile names.
- Include fields needed to distinguish bundled modern data from optional
  long-range DE441 data.
- Add a parser/validator in the app or ephemeris data layer without constructing
  ephemeris results.

Verification:
- Add manifest tests for valid manifests, missing required fields, checksum
  metadata, validity-range parsing, compression metadata, modern profile, and
  optional DE441 profile.

### HP-019: Add zstd archive handling and first-use activation

Status: [x]
Priority: Critical
Type: Data loading / archive
Source:
- Spec: Data Management; Build and Dependencies

Required work:
- Add zstd compressed data archive handling for ephemeris data assets.
- Decompress or memory-map assets into a writable application data cache before
  use.
- Do not place multi-gigabyte kernels in Qt resource files.
- Implement first-use activation from bundled external app resources into the
  app data cache.

Verification:
- Add tests for valid zstd archive activation, corrupt archive failure, checksum
  mismatch failure, idempotent first-use activation, and no Qt-resource path for
  large kernels.

### HP-020: Add ephemeris data cache metadata persistence

Status: [x]
Priority: Critical
Type: Data loading / cache metadata
Source:
- Spec: Settings; In-App Updates

Required work:
- Add `EphemerisDataCacheSnapshot` separate from star/deep-sky catalog cache
  snapshots.
- Persist installed kernel path/version, EOP path/version, leap-second table
  version, Delta T data version, data revision token, and last update result.
- Add clear-cache behavior that removes installed ephemeris data metadata and
  returns to bundled fallback metadata.

Verification:
- Add settings/cache tests for save/load, partial metadata, malformed metadata,
  clear-cache behavior, data revision persistence, and bundled fallback after
  clear.

### HP-021: Add `SkyEphemerisDataManager`

Status: [x]
Priority: Critical
Type: App data manager
Source:
- Spec: In-App Updates
- Current code: `SkyCatalogManager` is catalog-scoped.

Required work:
- Add an application-layer `SkyEphemerisDataManager` owned by
  `SkyContextController`, parallel to `SkyCatalogManager`.
- Report bundled and installed ephemeris data status.
- Provide active immutable data snapshot metadata to the engine factory.
- Preserve offline fallback to bundled data.
- Emit revision changes when active data changes.
- Do not parse star/deep-sky catalogs in this manager.

Verification:
- Add unit tests for initial bundled status, installed data status, missing
  installed data fallback, revision emission, and no interaction with
  `SkyCatalogManager` catalog state.

### HP-022: Add ephemeris update, cancellation, and atomic activation flow

Status: [x]
Priority: High
Type: App data manager / update flow umbrella
Source:
- Spec: Data Management; In-App Updates

Required work:
- Keep this as the parent task for ephemeris update flow behavior.
- Mark complete only when HP-022A through HP-022F are complete.
- Keep download UI outside the engine.

Verification:
- All HP-022 child verification items pass.

#### HP-022A: Add ephemeris update request and status API

Status: [x]
Priority: High
Type: App data manager / update API
Dependencies:
- HP-018
- HP-020
- HP-021

Required work:
- Add update/install actions for kernels, EOP data, leap-second data, and Delta
  T data to `SkyEphemerisDataManager`.
- Add request, progress, status, result, and cancel surfaces needed by the
  application layer.
- Keep QML and Preferences controls out of this subtask.

Verification:
- Add unit tests for update request validation, status transitions, progress
  reporting, cancel request signaling, and no interaction with
  `SkyCatalogManager` catalog state.

#### HP-022B: Add file-backed ephemeris download staging

Status: [x]
Priority: High
Type: App data manager / download staging
Dependencies:
- HP-018
- HP-021
- HP-022A

Required work:
- Download kernel, EOP, leap-second, and Delta T assets to staging files rather
  than memory buffers.
- Persist enough staged metadata to support resumable or restartable behavior.
- Resume downloads when supported by the server; otherwise restart cleanly from
  staged metadata.
- Keep staged data separate from the active data set.

Verification:
- Add tests for initial staged download, restart after partial download,
  metadata recovery, unsupported resume fallback, and active-data preservation
  while staging is incomplete.

#### HP-022C: Verify staged update sets before activation

Status: [x]
Priority: High
Type: App data manager / verification
Dependencies:
- HP-018
- HP-019
- HP-022B

Required work:
- Validate manifest-selected assets before activation.
- Verify checksums, expected component kinds, versions, validity ranges, and
  compression metadata.
- Reject incomplete, mismatched, malformed, or corrupt staged update sets.

Verification:
- Add tests for checksum failure, wrong component kind, missing asset, malformed
  metadata, corrupt compressed data, unsupported profile, and successful staged
  verification.

Notes:
- HP-022C implementation found there was no standalone staged update-set
  verification API yet; verification was only implicit during asset activation.
  The task therefore adds an explicit verification surface before activation.

#### HP-022D: Atomically activate verified ephemeris data

Status: [x]
Priority: High
Type: App data manager / atomic activation
Dependencies:
- HP-019
- HP-020
- HP-021
- HP-022C

Required work:
- Promote a complete verified staged update set into the application data cache
  atomically.
- Persist `EphemerisDataCacheSnapshot` only after activation succeeds.
- Emit ephemeris data revision changes only after successful activation.
- Ensure interrupted activation does not replace the active data set.

Verification:
- Add tests for successful activation, revision changes after activation,
  interrupted install, failed metadata persistence, and active-data preservation
  after activation failure.

#### HP-022E: Add cancellation and failed-update preservation semantics

Status: [x]
Priority: High
Type: App data manager / cancellation
Dependencies:
- HP-022B
- HP-022D

Required work:
- Support safe cancellation of active transfers and install jobs.
- Clean or retain partial staging according to the restart policy.
- Guarantee the active snapshot and revision are unchanged on cancellation,
  checksum failure, rejected verification, or interrupted install.

Verification:
- Add tests for cancellation during download, cancellation during verification,
  cancellation before activation, cancellation during activation, failure
  cleanup, retained resumable staging, and unchanged active-data revision.

Notes:
- Verified PASS after the pre-activation cancellation test was updated to
  cancel after staged verification succeeds and before install begins.

#### HP-022F: Add update-flow test harness and fault injection

Status: [x]
Priority: High
Type: Testing / update flow
Dependencies:
- HP-022A
- HP-022B
- HP-022C
- HP-022D
- HP-022E

Required work:
- Add a reusable test harness for ephemeris update transport, staging,
  verification, activation, cancellation, and failure injection.
- Cover checksum failure, interrupted install, cancellation, restart after
  partial download, successful activation, revision changes after activation,
  and active-data preservation after failure.

Verification:
- Update-flow tests deterministically exercise success, cancellation, restart,
  and all supported injected failures without network access.

### HP-023: Add CALCEPH kernel loading and selection provider

Status: [x]
Priority: Critical
Type: Data loading / kernel provider
Source:
- Spec: CALCEPH Solar-System Engine; Data Management

Required work:
- Add `CalcephKernelProvider` under the high-precision engine data-loading
  boundary.
- Load the selected modern kernel, such as DE440 or DE440s, from the active data
  snapshot.
- Select optional DE441 when installed and requested for long-range coverage.
- Report kernel version, source, validity range, and missing/out-of-range
  status.
- Do not compute solar-system coordinates in this task.

Verification:
- Add high-precision-only tests for kernel open/close, manifest selection,
  missing kernel, wrong checksum or wrong file, modern-range selection, optional
  DE441 selection, and validity-range reporting.

### HP-024: Package bundled modern ephemeris data as external app resources

Status: [x]
Priority: High
Type: Packaging / data loading
Source:
- Spec: Data Management; Build and Dependencies; Migration Phase 6

Required work:
- Add install/package rules for practical modern offline data: modern kernel
  when package size allows, leap-second table, EOP data with prediction
  interval, Delta T data/model, and manifest.
- Keep multi-gigabyte DE441 out of the default app bundle.
- Ensure bundled data is discoverable by `SkyEphemerisDataManager` on clean
  install.

Verification:
- Add packaging smoke checks for bundled manifest presence, bundled data
  discovery, first-run activation, and clean-install offline operation.
- Add platform packaging checks for macOS, Windows, and Linux release packages
  when release packaging is exercised.

## Phase 4: Numerical High-Precision Computation

### HP-025: Implement geometric solar-system vector calculation

Status: [x]
Priority: Critical
Type: Numerical / solar-system computation
Source:
- Spec: Goals; CALCEPH Solar-System Engine; Testing Requirements

Required work:
- Add `SolarSystemStateCalculator` using `CalcephKernelProvider`.
- Compute geometric vectors for supported major solar-system bodies from the
  selected JPL kernel.
- Produce geometric RA/Dec request outputs through the high-precision facade.
- Report unsupported bodies and out-of-kernel-range dates through structured
  result status and warnings.
- Do not implement apparent corrections in this task.

Verification:
- Add deterministic tests against compact Horizons ICRF/no-apparent-correction
  geometric fixtures.
- Add tests for unsupported body, missing kernel, out-of-range kernel date, and
  optional DE441 long-range coverage when installed.

### HP-026: Implement IAU/IERS frame transformation pipeline

Status: [x]
Priority: Critical
Type: Numerical / frame transforms umbrella
Source:
- Spec: Field Research Summary; Correction Options; Testing Requirements

Required work:
- Keep this as the parent task for SOFA/IERS frame transformation stages.
- Mark complete only when HP-026A through HP-026C are complete.
- Do not implement light-time, aberration, deflection, topocentric observer
  parallax, or refraction in this task.

Verification:
- All HP-026 child verification items pass.

#### HP-026A: Implement celestial frame transforms

Status: [x]
Priority: Critical
Type: Numerical / frame transforms
Dependencies:
- HP-003
- HP-004
- HP-005
- HP-014

Required work:
- Add celestial transform stages for ICRS, GCRS, and CIRS following SOFA/IERS
  conventions through ERFA/SOFA wrappers.
- Use `TimeScaleService` outputs required by the celestial transform stages.
- Keep the implementation behind the `FrameTransformer` boundary.

Verification:
- Add frame-transform tests against SOFA/ERFA reference values or fixtures for
  ICRS, GCRS, and CIRS stages.
- Add precision and round-trip tests where supported by the transform model.

#### HP-026B: Implement Earth rotation and terrestrial transforms

Status: [x]
Priority: Critical
Type: Numerical / Earth orientation transforms
Dependencies:
- HP-003
- HP-005
- HP-014
- HP-016

Required work:
- Add CIRS, TIRS, and ITRS transform stages using UT1 and interpolated EOP
  values.
- Return correction/status metadata for transforms that degrade because EOP or
  time data is missing, stale, predicted, or estimated.
- Keep observer-specific topocentric position and diurnal parallax work in
  HP-028.

Verification:
- Add frame-transform tests against SOFA/ERFA reference values or fixtures for
  CIRS, TIRS, and ITRS stages.
- Add tests that missing, stale, predicted, or estimated EOP data produces
  degraded transform metadata.

#### HP-026C: Add `FrameTransformer` orchestration and per-stage metadata

Status: [x]
Priority: Critical
Type: Numerical / frame transform API
Dependencies:
- HP-026A
- HP-026B

Required work:
- Add the public-internal `FrameTransformer` orchestration API used by the
  high-precision facade and apparent-place pipeline.
- Compose supported frame stages without duplicating time-scale or EOP lookup
  work per stage.
- Preserve per-stage status, warning, provenance, and applied-correction
  metadata for result assembly.

Verification:
- Add orchestration tests for multi-stage transforms, skipped/unavailable
  stages, metadata propagation, and degraded-stage aggregation.

### HP-027: Implement apparent-place correction pipeline

Status: [x]
Priority: Critical
Type: Numerical / apparent corrections umbrella
Source:
- Spec: Correction Options; Apparent and Topocentric Positions

Required work:
- Keep this as the parent task for geocentric apparent-place corrections.
- Mark complete only when HP-027A through HP-027G are complete.
- Keep topocentric observer parallax in HP-028 and atmospheric refraction in
  HP-029.

Verification:
- All HP-027 child verification items pass.

#### HP-027A: Add `ApparentPlaceCalculator` boundary and request mode routing

Status: [x]
Priority: Critical
Type: Numerical / apparent corrections
Dependencies:
- HP-004
- HP-005
- HP-006
- HP-009
- HP-025
- HP-026

Required work:
- Add `ApparentPlaceCalculator`.
- Route geometric, astrometric, and apparent RA/Dec request modes from
  `EphemerisRequest`.
- Keep this subtask focused on calculator boundaries and routing; do not
  implement numerical corrections yet.

Verification:
- Add tests proving request mode dispatch for geometric, astrometric, and
  apparent outputs.
- Add tests that unsupported request modes return structured status/warnings.

#### HP-027B: Implement solar-system light-time correction

Status: [x]
Priority: Critical
Type: Numerical / apparent corrections
Dependencies:
- HP-014
- HP-023
- HP-025
- HP-027A

Required work:
- Implement correction-flag-controlled solar-system light-time correction using
  CALCEPH body and Earth states.
- Use an iterative or explicitly documented approximation strategy appropriate
  to the target body and data set.
- Produce astrometric vectors for later apparent-place corrections.

Verification:
- Add tests with light-time enabled and disabled.
- Add fixture or reference tests for representative supported major bodies.
- Add tests that unavailable light-time inputs produce warnings rather than
  silent omission.

#### HP-027C: Implement stellar aberration and gravitational light deflection

Status: [x]
Priority: Critical
Type: Numerical / apparent corrections
Dependencies:
- HP-014
- HP-025
- HP-026
- HP-027B

Required work:
- Implement correction-flag-controlled stellar aberration.
- Implement correction-flag-controlled gravitational light deflection.
- Use solar-system state and frame/time inputs from the existing high-precision
  calculators and providers.

Verification:
- Add tests for aberration enabled and disabled.
- Add tests for gravitational deflection enabled and disabled.
- Add tests that requested-but-unavailable correction inputs produce warnings
  rather than silent omission.

#### HP-027D: Integrate precession and nutation into apparent RA/Dec

Status: [x]
Priority: Critical
Type: Numerical / apparent corrections
Dependencies:
- HP-014
- HP-016
- HP-026
- HP-027C

Required work:
- Use `FrameTransformer` outputs to apply precession and nutation stages needed
  for apparent RA/Dec.
- Respect correction flags controlling precession and nutation.
- Keep terrestrial/topocentric transforms out of this subtask unless needed as
  metadata inputs.

Verification:
- Add tests for precession and nutation enabled and disabled.
- Add tests that degraded time or EOP inputs propagate transform warnings into
  apparent-place metadata.

#### HP-027E: Implement annual parallax handling for apparent-place inputs

Status: [x]
Priority: Critical
Type: Numerical / apparent corrections
Dependencies:
- HP-007
- HP-025
- HP-027A
- HP-031

Required work:
- Implement correction-flag-controlled annual parallax where the input model
  supports it.
- Use catalog astrometry inputs for stars where available.
- Keep proper motion, radial velocity, and batch catalog-star propagation in
  HP-031 and HP-032.
- Return degraded status when annual parallax is requested but required source
  data is unavailable and fixed-coordinate fallback is used.

Verification:
- Add annual parallax enabled/disabled tests for objects with complete,
  partial, and missing parallax metadata.
- Add tests for warning metadata when requested parallax cannot be applied.

#### HP-027F: Record applied correction flags, warnings, and per-flag tests

Status: [x]
Priority: Critical
Type: Computation / result metadata
Dependencies:
- HP-005
- HP-027A
- HP-027B
- HP-027C
- HP-027D
- HP-027E

Required work:
- Record correction flags actually applied in result metadata.
- Distinguish requested, applied, skipped, and unavailable corrections.
- Add stable warning codes and text for requested-but-unavailable corrections.

Verification:
- Add tests for each correction flag enabled and disabled.
- Add tests that requested-but-unavailable corrections produce warnings rather
  than silent omission.
- Add tests for applied/skipped/unavailable correction metadata.

Notes:
- HP-027F implementation found the apparent-place pass-through path treated a
  missing frame transformer as an unavailable correction even when the requested
  output stayed in GCRS and no frame transform was needed. The fix keeps GCRS
  pass-through requests valid while still reporting unavailable frame-dependent
  corrections by specific flag.

#### HP-027G: Add apparent RA/Dec Horizons validation

Status: [x]
Priority: Critical
Type: Validation / apparent corrections
Dependencies:
- HP-025
- HP-026
- HP-027B
- HP-027C
- HP-027D
- HP-034

Required work:
- Add geocentric apparent RA/Dec validation against Horizons fixtures for
  supported major bodies.
- Keep topocentric observer/apparent and atmospheric refraction validation in
  HP-028, HP-029, and HP-037.
- Include fixture metadata identifying source parameters, frame, time scale,
  target, observer origin, expected values, and tolerance.

Verification:
- Apparent RA/Dec validation tests pass within documented tolerances.
- Fixture metadata completeness tests cover the apparent-place fixture set.

Notes:
- HP-027G implementation found the apparent-place path was routing
  precession/nutation output through CIO-based CIRS coordinates, which do not
  match Horizons apparent RA/Dec referenced to the true equator and equinox of
  date. The implementation added an explicit true-equator/equinox frame path
  for apparent RA/Dec validation.

### HP-028: Implement topocentric observer and diurnal parallax pipeline

Status: [x]
Priority: Critical
Type: Numerical / topocentric computation
Source:
- Spec: Goals; Correction Options; Testing Requirements

Required work:
- Add observer geocentric position calculation using observer latitude,
  longitude, elevation, UT1-UTC, and polar motion inputs.
- Implement diurnal/topocentric parallax when requested and observer data is
  valid.
- Produce topocentric apparent positions and horizontal coordinates through the
  request path.
- Keep atmospheric refraction separate.

Verification:
- Add topocentric Moon, Sun, and planet fixture tests against Horizons
  observer/apparent quantities.
- Add tests for invalid observer, missing EOP data, parallax enabled/disabled,
  and elevation effects.

### HP-029: Implement atmospheric refraction calculation

Status: [x]
Priority: High
Type: Numerical / refraction
Source:
- Spec: Correction Options; In-App Updates; Testing Requirements

Required work:
- Add `AtmosphericRefractionCalculator`.
- Apply refraction only when requested and observer/atmosphere inputs are
  available.
- Support refraction disabled behavior explicitly.
- Report when refraction is requested but inputs are missing or out of accepted
  range.

Verification:
- Add tests for refraction enabled, disabled, missing atmosphere inputs,
  boundary altitude behavior, and warning metadata.

### HP-030: Implement high-precision result assembly

Status: [x]
Priority: Critical
Type: Computation / result assembly
Source:
- Spec: Result Status; Accuracy and Ranges

Required work:
- Add `EphemerisResultBuilder`.
- Combine calculator outputs, data-source provenance, validity ranges,
  uncertainty estimates when available, warning codes/text, and applied
  correction flags into high-precision results.
- Ensure out-of-range requests return degraded fallback results where possible
  instead of hard failure.
- Ensure failures remain explicit when no fallback result can be produced.

Verification:
- Add result assembly tests for valid, degraded, unsupported, out-of-range, and
  failed results.
- Add tests for missing DE441, stale EOP data, stale leap-second data, ancient
  Delta T assumptions, and unsupported bodies.

### HP-031: Implement single-star astrometry propagation

Status: [x]
Priority: High
Type: Numerical / stellar astrometry
Source:
- Spec: Correction Options; Migration Phase 5

Required work:
- Add `StarAstrometryCalculator`.
- Propagate catalog stars using proper motion, stellar parallax, and radial
  velocity where source data provides them.
- Respect correction flags controlling proper motion, radial velocity, and
  stellar parallax.
- Return degraded status when required astrometry fields are missing and a fixed
  coordinate fallback is used.

Verification:
- Add single-star propagation fixture tests with full astrometry, partial
  astrometry, fixed-only stars, and correction flags enabled/disabled.

Notes:
- HP-031 implementation found the public `CelestialBody` model still lacked a
  catalog-star astrometry payload, so this task adds the missing payload needed
  by the calculator instead of treating HP-007 as a separate blocker.

### HP-032: Add high-throughput catalog-star batch path

Status: [x]
Priority: High
Type: Performance / numerical batching umbrella
Source:
- Spec: Performance Requirements

Required work:
- Keep this as the parent task for high-throughput catalog-star computation.
- Mark complete only when HP-032A through HP-032C are complete.

Verification:
- All HP-032 child verification items pass.

#### HP-032A: Add immutable cache-friendly catalog astrometry arrays

Status: [x]
Priority: High
Type: Performance / catalog data layout
Dependencies:
- HP-007
- HP-031

Required work:
- Preprocess immutable catalog astrometry into cache-friendly arrays.
- Preserve fixed-coordinate fallback data for stars without full astrometry.
- Keep the data layout immutable after construction so it can be shared by
  read-only computations.

Verification:
- Add tests for array construction from full astrometry, partial astrometry,
  and fixed-only catalog bodies.
- Add tests that immutable array snapshots remain valid after source catalog
  lifetime changes.

#### HP-032B: Add batch star astrometry propagation

Status: [x]
Priority: High
Type: Performance / numerical batching
Dependencies:
- HP-031
- HP-032A

Required work:
- Add a batch path for catalog-star proper-motion, stellar parallax, and radial
  velocity propagation.
- Respect the same correction flags and fallback semantics as single-star
  propagation.
- Keep batch results numerically consistent with `StarAstrometryCalculator`
  single-star outputs.

Verification:
- Add correctness tests proving batch results match single-star propagation
  within the same tolerance.
- Add tests for full astrometry, partial astrometry, fixed-only stars, and
  correction flags enabled/disabled.

#### HP-032C: Integrate batch path into full-frame computation

Status: [x]
Priority: High
Type: Performance / full-frame integration
Dependencies:
- HP-026
- HP-030
- HP-032B

Required work:
- Apply shared transform state once per frame/request for the full-frame
  catalog-star path.
- Avoid per-object heap allocations in the full-frame star path where
  practical.
- Integrate the batch path with high-precision snapshot computation without
  changing single-object request semantics.
- Coordinate with HP-033 if shared request state is centralized in
  `EphemerisComputationCache`.

Verification:
- Add large-catalog performance tests or benchmarks with a documented
  representative catalog size.
- Compare batch path timing or allocation counts against a simple per-object
  baseline.
- Add tests proving shared transform state is reused without stale results.

### HP-033: Add high-precision computation cache and read-only thread safety

Status: [x]
Priority: High
Type: Performance / concurrency
Source:
- Spec: Performance Requirements

Required work:
- Add `EphemerisComputationCache` for shared per-request state: time-scale
  conversions, Earth state, observer geocentric position, Earth-orientation
  matrices, precession/nutation matrices, apparent-place constants, and
  atmospheric constants.
- Make constructed high-precision engines safe for concurrent read-only
  computations.
- Ensure mutable data updates create a new immutable data snapshot or new engine
  instance rather than mutating active computation data in place.

Verification:
- Add tests that repeated full-frame computations reuse shared state without
  stale results.
- Add concurrent read-only compute tests.
- Add revision-isolation tests proving old and new data snapshots do not mutate
  each other.

## Phase 5: Test Fixtures And Validation Infrastructure

### HP-034: Add ephemeris fixture infrastructure and LFS policy

Status: [x]
Priority: Critical
Type: Validation infrastructure
Source:
- Spec: Testing Requirements; Reference Fixture Policy
- Current code: fixtures exist under `tests/fixtures/catalogs/`; no
  `tests/fixtures/ephemeris/` directory exists.

Required work:
- Add `libs/skygate-ephemeris/tests/fixtures/ephemeris/`.
- Define compact JSON or CSV fixture metadata fields: source URL/API
  parameters, generated date, source frame, time scale, target, observer,
  expected values, and tolerance.
- Add fixture loading helpers and shared angular tolerance utilities.
- Update Git LFS rules so large fixtures and kernels use LFS while at least one
  small CI smoke fixture can remain non-LFS as required by the spec.

Verification:
- Add tests for fixture parser success/failure and metadata completeness.
- Verify the non-LFS smoke fixture is available in a normal checkout.

### HP-035: Add time and EOP validation test targets

Status: [x]
Priority: Critical
Type: Validation
Source:
- Spec: Testing Requirements

Required work:
- Register Qt Test targets for time primitives, time-scale conversion,
  leap-second behavior, Delta T, EOP loading, and EOP interpolation.
- Keep these tests independent of CALCEPH kernels.

Verification:
- Tests cover UTC/TAI/TT/TDB/UT1 conversions, leap-second edges, BCE/no-year-zero
  conversion, EOP interpolation, and EOP fallback warnings.

### HP-036: Add kernel and geometric solar-system validation targets

Status: [x]
Priority: Critical
Type: Validation
Source:
- Spec: Testing Requirements; Field Research Summary

Required work:
- Register high-precision-only tests for CALCEPH kernel loading and geometric
  solar-system vectors.
- Use Horizons ICRF vectors with no apparent corrections for geometric fixtures.
- Include a small smoke fixture that does not require full production kernels.

Verification:
- Tests pass with high precision enabled and required smoke data installed.
- Tests are skipped or excluded with a clear reason when high precision is
  disabled.

### HP-037: Add apparent/topocentric and correction validation targets

Status: [x]
Priority: Critical
Type: Validation
Source:
- Spec: Testing Requirements; Field Research Summary

Required work:
- Register tests for apparent RA/Dec, topocentric Moon/Sun/planet positions,
  correction flag behavior, and atmospheric refraction.
- Use Horizons observer/apparent quantities for end-to-end apparent-position
  validation.

Verification:
- Tests cover corrections enabled/disabled, refraction enabled/disabled,
  invalid observer behavior, missing EOP/leap-second degraded warnings, and
  fixture tolerances.

### HP-038: Add factory, warning, and fallback validation targets

Status: [x]
Priority: Critical
Type: Validation
Source:
- Spec: Testing Requirements; Factory; Result Status

Required work:
- Register tests for engine factory strict/fallback behavior and result status
  warnings.
- Keep creation-policy tests separate from numerical CALCEPH tests by using
  fake data snapshots where possible.

Verification:
- Tests cover simple creation, high-precision unavailable fallback, strict
  failure, missing DE441 degraded warning, stale data warnings, unsupported
  body, out-of-range request, and failed request.

## Phase 6: App Integration, Settings, Preferences, And UI

### HP-039: Add ephemeris user settings snapshot

Status: [x]
Priority: High
Type: App settings
Source:
- Spec: Engine Selection; Settings
- Current code: `SkySettingsStore::StateSnapshot` has catalog and UI settings
  only.

Required work:
- Add user-facing ephemeris settings for selected engine kind, correction flags
  or preset, refraction enabled/disabled, atmosphere defaults, preferred
  kernel/data profile, online update enabled/disabled, and ephemeris update URLs
  or preset.
- Keep installed data/cache metadata in `EphemerisDataCacheSnapshot`, not in the
  user settings snapshot.
- Add codecs and safe malformed-value fallback.

Verification:
- Add settings round-trip tests, malformed setting fallback tests, partial-state
  tests, and split/merge codec tests.

Notes:
- HP-039 implementation found cache-only ephemeris data persistence can create
  a loadable state snapshot through the shared settings version key. The fix
  records whether ephemeris user settings are actually present before applying
  engine defaults during controller settings load.

### HP-040: Decouple engine ownership from catalog rebuilds

Status: [x]
Priority: Critical
Type: App architecture
Source:
- Spec: Current State; Factory; In-App Updates
- Current code: `SkyCatalogRuntime` rebuilds the active catalog and engine
  together.

Required work:
- Move selected engine kind/options/data wiring out of active catalog rebuild
  ownership.
- Keep catalog rebuilds responsible for active star/deep-sky body lists.
- Rebuild or replace the ephemeris engine when engine kind, options, or active
  ephemeris data changes.
- Preserve selected engine settings when star/deep-sky catalogs change.

Verification:
- Add tests proving catalog changes preserve engine settings and active
  ephemeris data selection.
- Add tests proving engine changes do not re-download or reparse star/deep-sky
  catalogs.

### HP-041: Apply selected engine and request options to every position consumer

Status: [x]
Priority: High
Type: App integration umbrella
Source:
- Spec: Engine Selection

Required work:
- Keep this as the parent task for selected-engine/request-option app
  integration.
- Mark complete only when HP-041A through HP-041H are complete.
- Keep `SkyObjectSearchModel` catalog/label-only unless position-dependent
  search is introduced separately.
- Keep cache-key expansion in HP-042.

Verification:
- All HP-041 child verification items pass.

#### HP-041A: Build app-level ephemeris request context

Status: [x]
Priority: High
Type: App integration / request context
Dependencies:
- HP-006
- HP-039
- HP-040

Required work:
- Centralize conversion from selected engine settings, active data metadata,
  observer, time, and `SkyContext` into the `EphemerisRequest` form used by app
  consumers.
- Include selected high-precision correction options and astronomical epoch
  where available.
- Preserve simple-engine compatibility for consumers that still use
  `core::SkyContext` adapters.

Verification:
- Add tests for request-context construction from settings, observer/time,
  correction options, refraction settings, and simple-engine defaults.

#### HP-041B: Apply selected engine and request to frame rendering

Status: [x]
Priority: High
Type: App integration / rendering
Dependencies:
- HP-040
- HP-041A

Required work:
- Route the selected engine consistently to frame rendering.
- Update scene snapshot computation to use the request-based API where
  high-precision correction options or astronomical epoch are required.
- Do not extend scene cache keys in this subtask; keep that work in HP-042.

Verification:
- Add integration tests using fake Simple and HighPrecision engines with
  distinct coordinates.
- Assert frame rendering switches when engine kind or request options change.

#### HP-041C: Apply selected engine and request to search focus and tracking

Status: [x]
Priority: High
Type: App integration / search and tracking
Dependencies:
- HP-041A
- HP-041B

Required work:
- Route search focus, tracked-target recentering, and tracking computations
  through the selected engine/request context.
- Keep `SkyObjectSearchModel` catalog/label-only unless position-dependent
  search is introduced separately.

Verification:
- Add integration tests proving search focus and tracking use coordinates from
  the selected engine.
- Add tests proving catalog/label-only search behavior remains unchanged.

#### HP-041D: Apply selected engine and request to inspector and observation events

Status: [x]
Priority: High
Type: App integration / inspection and events
Dependencies:
- HP-041A
- HP-041B

Required work:
- Route object inspection calculations through the selected engine/request
  context.
- Update observation event calculations so rise, set, culmination, and related
  observer/time-dependent values use the same selected engine/options as
  rendering.

Verification:
- Add integration tests proving inspector coordinates and event summaries switch
  when engine kind or request options change.
- Add tests for event calculation request-option propagation.

#### HP-041E: Apply selected engine and request to trails

Status: [x]
Priority: High
Type: App integration / trails
Dependencies:
- HP-041A
- HP-041B

Required work:
- Route trail sampling through the selected engine/request context.
- Preserve selected correction options across sampled epochs while updating the
  epoch per sample.
- Keep trail behavior compatible with the simple engine.

Verification:
- Add integration tests proving trail samples switch when engine kind or request
  options change.
- Add tests that sampled requests preserve correction options and update epochs.

#### HP-041F: Apply selected engine and request to night conditions

Status: [x]
Priority: High
Type: App integration / night conditions
Dependencies:
- HP-041A
- HP-041D

Required work:
- Route night-conditions calculations through the selected engine/request
  context.
- Ensure Sun/Moon state, twilight, moonrise, and related event calculations
  switch with engine kind and options.

Verification:
- Add integration tests proving night-conditions icon/state and summary values
  switch when engine kind or request options change.

#### HP-041G: Reconcile observer/time-dependent reference overlays

Status: [x]
Priority: High
Type: App integration / reference overlays
Dependencies:
- HP-041A

Required work:
- Review reference overlay computations that depend on observer, time, or
  apparent frame assumptions.
- Route overlays through the selected engine/request context only where the
  high-precision engine should be the single source of truth.
- Leave purely geometric reference overlays outside the engine path when they do
  not depend on ephemeris calculations.

Verification:
- Add tests documenting which reference overlays use the selected engine/request
  and which remain pure reference math.
- Add integration tests for any observer/time-dependent overlays that route
  through the selected engine.

#### HP-041H: Add selected-engine integration test matrix

Status: [x]
Priority: High
Type: Testing / app integration
Dependencies:
- HP-041B
- HP-041C
- HP-041D
- HP-041E
- HP-041F
- HP-041G

Required work:
- Add an integration test matrix using fake Simple and HighPrecision engines
  with distinct coordinates.
- Assert render, search focus, tracking, inspector, trails, night conditions,
  event calculations, and applicable reference overlays switch together when
  engine kind/options change.
- Keep cache invalidation assertions for high-precision context fields in
  HP-042.

Verification:
- The selected-engine integration matrix passes for both simple and
  high-precision fake engines and covers option changes separately from catalog
  changes.

### HP-042: Extend scene and app cache keys for high precision

Status: [x]
Priority: High
Type: App cache invalidation
Source:
- Spec: Performance Requirements
- Current code: scene snapshot cache includes catalog revision, observer, UTC
  time, and engine pointer identity.

Required work:
- Extend scene snapshot inputs/cache keys with astronomical epoch, engine kind,
  engine options revision, ephemeris data revision, EOP/leap-second data
  revision, and observer/refraction inputs used by high precision.
- Ensure data/option changes invalidate snapshots without forcing unrelated
  render-frame churn.
- Include the same high-precision context fields in any future event or
  night-condition cache.

Verification:
- Add scene pipeline tests for invalidation on engine kind, correction options,
  astronomical epoch/subsecond changes, ephemeris data revision, EOP revision,
  leap-second revision, and refraction inputs.
- Add render-only change tests proving projection/theme/view changes do not
  recompute ephemeris snapshots unnecessarily.

### HP-043: Add Preferences engine selector and option controls

Status: [x]
Priority: High
Type: UI / settings
Source:
- Spec: Engine Selection; Correction Options; In-App Updates
- Current code: `PreferencesCatalogSection.qml` exposes star/deep-sky catalog
  controls only.

Required work:
- Add exactly one top-level engine selector with `Simple` and `High precision`.
- Add high-precision correction option controls or presets while preserving
  explicit flags in the engine API.
- Add refraction enabled/disabled and atmosphere preset/input controls.
- Wire controls through `PreferencesDraft.qml`, `SkyContextController`, and
  ephemeris user settings.
- Show high-precision-only controls only when High precision is selected.

Verification:
- Add QML tests for selector binding, exactly two choices, high-precision
  control visibility, draft reset/apply, settings persistence, and malformed
  saved setting fallback.

### HP-044: Add Preferences ephemeris data controls

Status: [x]
Priority: High
Type: UI / data controls
Source:
- Spec: In-App Updates

Required work:
- Add an `Ephemeris Data` group on the existing Catalog page or rename the page
  to `Data` if needed.
- Show modern kernel status, long-range DE441 installed status, EOP status,
  leap-second status, Delta T status, and last update result.
- Add update actions for ephemeris data managed by `SkyEphemerisDataManager`.
- Add offline/online update mode and clear ephemeris data cache controls.
- Keep implementation of downloads/checksums/atomic activation inside
  `SkyEphemerisDataManager`, not QML.

Verification:
- Add QML tests for status text, update button enabled states, clear-cache
  action invocation, offline fallback display, long-range data installed/absent
  display, and no QML warnings.

### HP-045: Surface degraded-result warnings and provenance in UI

Status: [x]
Priority: High
Type: UI / result presentation
Source:
- Spec: Result Status; Testing Requirements

Required work:
- Extend scene/selection/status payloads to carry high-precision warning text,
  status, provenance, validity range, uncertainty when available, and applied
  correction summary where appropriate.
- Display degraded/out-of-range warning text with tooltips in object inspector
  or relevant status surfaces.
- Do not show high-precision-only metadata for simple-engine results unless the
  compatibility adapter provides it.

Verification:
- Add UI tests for degraded-result warning display, tooltip text, out-of-range
  messaging, missing DE441 messaging, and provenance/range display where
  surfaced.

## Phase 7: Packaging, Release Policy, And Final Acceptance

### HP-046: Enable high precision for release builds while preserving simple-only developer builds

Status: [x]
Priority: High
Type: Build / release policy
Source:
- Spec: Build and Dependencies; Acceptance Criteria

Required work:
- Configure release presets or release packaging so high precision is enabled
  when dependencies and bundled data are available.
- Keep developer/simple-only builds explicit and documented through
  `SKYGATE_ENABLE_HIGH_PRECISION_EPHEMERIS=OFF`.
- Ensure high-precision tests are present and runnable in vcpkg-enabled presets.

Verification:
- Configure release build with high precision enabled.
- Configure developer simple-only build with high precision disabled.
- Run high-precision test targets in a high-precision preset and simple-engine
  tests in both presets.

### HP-047: Validate clean-install offline behavior and optional DE441 flow

Status: [x]
Priority: High
Type: Packaging / acceptance
Source:
- Spec: Data Management; Acceptance Criteria

Required work:
- Verify a clean install can run high precision in the bundled modern range
  without network access.
- Verify dates outside the bundled kernel range degrade with explicit warning
  when DE441 is absent.
- Verify installing optional DE441 enables the requested long-range mode where
  data permits.
- Verify clearing ephemeris data returns to bundled fallback behavior.

Verification:
- Add packaging or integration smoke tests for clean install, offline bundled
  data, absent-DE441 degraded warning, installed-DE441 long-range selection, and
  cache clear fallback.

### HP-048: Final acceptance test matrix

Status: [x]
Priority: Critical
Type: Acceptance validation
Source:
- Spec: Acceptance Criteria

Required work:
- Build a final test matrix covering engine selection persistence,
  CALCEPH-backed solar-system RA/Dec, correction options, degraded fallbacks,
  bundled offline modern data, independent ephemeris updates, deterministic
  fixtures, and full-frame/search shared-state performance.
- Keep the matrix as verification of completed implementation tasks, not as a
  separate implementation path.

Verification:
- Users can choose Simple or High precision in Preferences and the choice
  persists across restarts.
- High precision computes solar-system RA/Dec through CALCEPH-backed kernels.
- Optional DE441 enables long-range coverage where data permits.
- Missing or stale data produces explicit degraded warnings.
- Callers can request geometric, astrometric, and apparent/topocentric outputs.
- Preferences update ephemeris data independently of star/deep-sky catalogs.
- Full-frame rendering and search avoid recomputing shared high-precision state
  per object.
- Simple engine tests still pass.

### HP-060: Wire production ephemeris data acquisition

Status: [x]
Priority: Critical
Type: Packaging / data delivery / UI follow-up
Source:
- Gap found after HP-044, HP-047, and HP-048: Preferences exposes update and
  install actions, but packaged app startup does not load an ephemeris data
  manifest or provide a real download/staging source for kernel assets.
- Current code: `SkyContextController::ephemerisDataUpdateEnabled()` requires a
  non-null data manifest and non-empty update resource root, while production
  `main.cpp` constructs `SkyContextController` without ephemeris factory inputs.

Required work:
- Add a production ephemeris data manifest asset with modern bundled data and
  optional DE441 long-range profile metadata, including source URLs, checksums,
  compression metadata, versions, validity ranges, and provenance.
- Load and parse the manifest during packaged app startup, report parse/load
  failures clearly, and pass the manifest, bundled resource root, and writable
  cache root through `SkyContextController::InitializationOptions`.
- Ensure clean installs have a usable bundled modern high-precision data path
  when release packaging includes the modern kernel, leap-second table, EOP
  data, and Delta T data.
- Change Preferences update actions from staged-only activation to a real
  profile acquisition flow: download each missing profile asset from
  `sourceUrl`, stage it in a file-backed update area, verify size/checksum and
  compression metadata, atomically activate it, then rebuild the active
  ephemeris engine.
- Keep local staged-resource activation available for tests and offline package
  fixtures, but do not require normal users to create staging directories or
  know about manifests.
- Add progress, cancellation, and actionable failure text for downloading,
  verifying, activating, and manifest-unavailable states.
- Disable update buttons only while an operation is running or when a clear
  unavailable reason is shown; avoid silent inactive buttons.
- Ensure `Install DE441` requests the explicit `de441-long-range` profile and
  does not silently activate or select a different solar-system kernel when
  that profile is missing or unavailable.
- Keep large optional DE441 kernels out of the default bundle unless release
  packaging explicitly opts into a separate long-range data package.

Verification:
- Packaged app startup loads the bundled manifest and enables ephemeris update
  controls when online updates are enabled and source URLs are available.
- A clean packaged install can use bundled modern high-precision data offline
  without pressing an update button.
- Pressing `Install DE441` downloads, verifies, activates, and selects the
  explicit long-range profile, then updates Preferences status text.
- Corrupt downloads, checksum mismatches, missing source URLs, missing
  manifests, canceled downloads, and activation failures show non-empty UI
  diagnostics and preserve the active data set.
- Clearing the ephemeris data cache returns to bundled modern fallback and keeps
  Preferences controls usable.
- Integration or packaging smoke tests cover macOS, Windows, and Linux startup
  resource paths where release packaging is exercised.
