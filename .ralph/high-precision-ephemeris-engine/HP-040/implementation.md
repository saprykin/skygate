## Task
- ID: HP-040
- Title: Decouple engine ownership from catalog rebuilds

## Status
READY

## Acceptance criteria claimed
- [x] Active catalog building no longer creates or returns an ephemeris engine
- [x] `SkyCatalogRuntime` and `SkyCatalogManager` no longer own or expose the engine
- [x] `SkyContextController` owns the current engine and rebuilds it from active catalog bodies and active ephemeris data
- [x] Catalog rebuilds preserve active ephemeris data selection
- [x] Focused catalog/runtime/ephemeris data manager tests pass
- [x] Full suite run completed with one unrelated QML failure recorded as HP-057

## Files changed
- `apps/skygate-ui/src/app/SkyContextController.cpp`
- `apps/skygate-ui/src/app/SkyContextController.hpp`
- `apps/skygate-ui/src/catalog/SkyActiveCatalogBuilder.cpp`
- `apps/skygate-ui/src/catalog/SkyActiveCatalogBuilder.hpp`
- `apps/skygate-ui/src/catalog/SkyCatalogManager.cpp`
- `apps/skygate-ui/src/catalog/SkyCatalogManager.hpp`
- `apps/skygate-ui/src/catalog/SkyCatalogRuntime.cpp`
- `apps/skygate-ui/src/catalog/SkyCatalogRuntime.hpp`
- `apps/skygate-ui/tests/catalog/SkyCatalogRuntimeTests.cpp`
- `apps/skygate-ui/tests/ephemeris/SkyEphemerisDataManagerTests.cpp`

## Important notes
- Reconfigured `build-ralph` with `SKYGATE_BUILD_UI=ON` to verify the UI task in the required build directory.
- Focused verification passed:
  `ctest --test-dir build-ralph -R 'skygate-ui-(sky-catalog-runtime|sky-ephemeris-data-manager|active-catalog-builder)-tests' --output-on-failure`.
- Full verification command:
  `ctest --test-dir build-ralph --output-on-failure`.
- Full verification result: 119/120 tests passed, with
  `skygate-ui-qml-main-window-tests` failing in
  `QmlMainWindowTests::footerPopupToolbarToggleClickClosesPopupAndTogglesToolbar()`.
- The unrelated QML failure was added to `IMPLEMENTATION_PLAN.md` as HP-057.

## Review fixes

- Review verdict addressed: NEEDS_FIX
- Findings addressed:
  - High-precision engine selection is downgraded during controller rebuild
    - Action: Fixed
    - Notes: `SkyContextController` now carries the high-precision factory
      dependency bundle from initialization into every rebuild request,
      including data manifest, data-set info, time-scale service,
      Earth-orientation provider, CALCEPH runtime, and diagnostics sink. When a
      rebuild would create a simple fallback for an already-active
      high-precision engine, the controller retains the high-precision engine
      instead of silently overwriting it.
- Files changed during fix pass:
  - `apps/skygate-ui/src/app/SkyContextController.cpp`
  - `apps/skygate-ui/src/app/SkyContextController.hpp`
  - `apps/skygate-ui/tests/ephemeris/SkyEphemerisDataManagerTests.cpp`
  - `.ralph/high-precision-ephemeris-engine/HP-040/implementation.md`
  - `.ralph/high-precision-ephemeris-engine/HP-040/fix.md`
- Tests run after fix:
  - `cmake --build build-ralph --target skygate-ui-sky-ephemeris-data-manager-tests -j2`: PASS
  - `ctest --test-dir build-ralph -R 'skygate-ui-(sky-catalog-runtime|sky-ephemeris-data-manager|active-catalog-builder)-tests' --output-on-failure`: PASS
  - `cmake --build build-ralph -j2`: PASS
  - `ctest --test-dir build-ralph --output-on-failure`: FAIL, 119/120 tests
    passed; only the known out-of-scope `skygate-ui-qml-main-window-tests`
    failure from HP-057 failed at
    `QmlMainWindowTests::footerPopupToolbarToggleClickClosesPopupAndTogglesToolbar()`.
- Remaining concerns:
  - The HP-057 QML main-window failure remains out of scope for HP-040.
