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
