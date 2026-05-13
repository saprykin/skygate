## Task
- ID: HP-008D
- Title: Implement factory selection for simple and unavailable high precision

## Status
READY

## Acceptance criteria claimed
- [x] Factory request selecting `Simple` creates a simple engine.
- [x] High-precision requests create a simple fallback only when fallback is explicitly allowed.
- [x] Strict high-precision requests return a structured unavailable error and no engine.
- [x] Requested catalog bodies and engine options propagate into created simple/fallback engines.
- [x] Diagnostics include non-empty high-precision-unavailable text.
- [x] Focused factory/API tests pass.

## Files changed
- `libs/skygate-ephemeris/src/engine/simple/SimpleEphemerisEngine.cpp`
- `libs/skygate-ephemeris/tests/CMakeLists.txt`
- `libs/skygate-ephemeris/tests/engine/EphemerisEngineFactorySelectionTests.cpp`

## Important notes
- Full `ctest --test-dir build-ralph --output-on-failure` reports 104/105 tests passing. The only failure is the pre-existing unrelated `skygate-ui-qml-main-window-tests` failure in `QmlMainWindowTests::footerPopupToolbarToggleClickClosesPopupAndTogglesToolbar()`, already tracked by HP-052.

## Review fixes
- Review verdict addressed: NEEDS_FIX
- Findings addressed:
  - Finding title: High-precision fallback is allowed by default
  - Action: Fixed
  - Notes: `EphemerisEngineFactoryRequest::fallbackPolicy` now defaults to `StrictHighPrecision`, so high-precision requests only create a simple fallback when callers explicitly select `AllowSimpleEngineFallback`. API default coverage was updated, and factory selection coverage now verifies that a default high-precision request returns `FailedStrictHighPrecisionUnavailable` with no engine.
- Files changed during fix pass:
  - `libs/skygate-ephemeris/include/skygate/ephemeris/EphemerisEngineFactory.hpp`
  - `libs/skygate-ephemeris/tests/engine/EphemerisApiModelTests.cpp`
  - `libs/skygate-ephemeris/tests/engine/EphemerisEngineFactorySelectionTests.cpp`
  - `.ralph/high-precision-ephemeris-engine/HP-008D/implementation.md`
  - `.ralph/high-precision-ephemeris-engine/HP-008D/fix.md`
- Tests run after fix:
  - `clang-format -i libs/skygate-ephemeris/include/skygate/ephemeris/EphemerisEngineFactory.hpp libs/skygate-ephemeris/tests/engine/EphemerisApiModelTests.cpp libs/skygate-ephemeris/tests/engine/EphemerisEngineFactorySelectionTests.cpp`: PASS
  - `cmake --build build-ralph --target skygate-ephemeris-engine-factory-selection-tests skygate-ephemeris-api-model-tests`: PASS
  - `ctest --test-dir build-ralph -R "skygate-ephemeris-(api-model|engine-factory-selection)-tests" --output-on-failure`: PASS
  - `ctest --test-dir build-ralph -R "skygate-ephemeris-(api-model|engine-interface-migration|engine-factory-selection)-tests" --output-on-failure`: PASS
  - `ctest --test-dir build-ralph --output-on-failure`: FAIL, with only the pre-existing unrelated `skygate-ui-qml-main-window-tests` failure already tracked by HP-052.
- Remaining concerns: Full-suite CTest still has the unrelated HP-052 QML main-window failure.
