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
