## Task
- ID: HP-006B
- Title: Add request-based snapshot compute API

## Status
READY

## Acceptance criteria claimed
- [x] `IEphemerisEngine::compute(const EphemerisRequest&)` added to the public interface
- [x] Simple engine request path implemented using the request epoch and context observer fields
- [x] Existing `SkySnapshot` shape preserved
- [x] Request-based snapshot test added for equivalence with the `SkyContext` path
- [x] Touched C++ files formatted with `clang-format`
- [x] Full build completed in `build-ralph`

## Files changed
- `libs/skygate-ephemeris/include/skygate/ephemeris/IEphemerisEngine.hpp`
- `libs/skygate-ephemeris/src/engine/simple/SimpleEphemerisEngine.cpp`
- `libs/skygate-ephemeris/tests/engine/EphemerisEngineBaselineTests.cpp`

## Important notes
- `ctest --test-dir build-ralph -R '^skygate-ephemeris-engine-baseline-tests$' --output-on-failure` passed.
- `ctest --test-dir build-ralph --output-on-failure` passed 102/103 tests. The only failure was the pre-existing and already tracked HP-051 QML footer popup toolbar regression in `skygate-ui-qml-main-window-tests`.
