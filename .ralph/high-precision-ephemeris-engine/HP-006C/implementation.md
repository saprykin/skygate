## Task
- ID: HP-006C
- Title: Add request-based body lookup overloads

## Status
READY

## Acceptance criteria claimed
- [x] `IEphemerisEngine` exposes request-based body lookup by id
- [x] `IEphemerisEngine` exposes request-based body lookup by index
- [x] Simple engine preserves current body-id and body-index lookup semantics on the request path
- [x] Request-based missing-body id and index cases return no result
- [x] Tests added for request-based body lookup by id, by index, and missing bodies
- [x] Touched C++ files formatted with `clang-format`

## Files changed
- `libs/skygate-ephemeris/include/skygate/ephemeris/IEphemerisEngine.hpp`
- `libs/skygate-ephemeris/src/engine/simple/SimpleEphemerisEngine.cpp`
- `libs/skygate-ephemeris/tests/engine/EphemerisEngineBaselineTests.cpp`

## Important notes
- `cmake --build build-ralph` completed successfully.
- `ctest --test-dir build-ralph -R '^skygate-ephemeris-engine-baseline-tests$' --output-on-failure` passed.
- `ctest --test-dir build-ralph --output-on-failure` passed 102/103 tests. The only failure was the existing unrelated `HP-051` QML failure in `skygate-ui-qml-main-window-tests`, `QmlMainWindowTests::footerPopupToolbarToggleClickClosesPopupAndTogglesToolbar()`.
