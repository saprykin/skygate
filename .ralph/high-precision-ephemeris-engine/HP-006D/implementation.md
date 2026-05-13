## Task
- ID: HP-006D
- Title: Convert `SkyContext` methods into compatibility adapters

## Status
READY

## Acceptance criteria claimed
- [x] Existing `core::SkyContext` overloads remain callable
- [x] Simple-engine `core::SkyContext` overloads construct an `EphemerisRequest`
      using the engine's configured options
- [x] Explicit `EphemerisRequest` paths continue to work for snapshots and body
      lookup
- [x] Tests cover the compatibility path applying engine default options
- [x] Touched C++ files formatted with `clang-format`

## Files changed
- `libs/skygate-ephemeris/src/engine/simple/SimpleEphemerisEngine.cpp`
- `libs/skygate-ephemeris/tests/engine/EphemerisEngineBaselineTests.cpp`

## Important notes
- `cmake --build build-ralph --target skygate-ephemeris-engine-baseline-tests`: PASS
- `ctest --test-dir build-ralph -R '^skygate-ephemeris-engine-baseline-tests$' --output-on-failure`: PASS
- `cmake --build build-ralph`: PASS
- `ctest --test-dir build-ralph --output-on-failure`: FAIL, with only the pre-existing HP-051 `skygate-ui-qml-main-window-tests` footer popup toolbar failure.
