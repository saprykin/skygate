## Task
- ID: HP-005
- Title: Add public result status and provenance model

## Status
READY

## Acceptance criteria claimed
- [x] Public result status enum added with valid, degraded, unsupported, out-of-range, and failed states
- [x] Warning model added with stable warning codes and non-empty default display text
- [x] Result metadata added for provenance, effective validity range, estimated angular uncertainty, and applied correction flags
- [x] `CelestialBodyState` keeps existing coordinate fields readable while exposing result metadata
- [x] Simple-engine states populate provenance and structured degraded/unsupported status where current behavior returns partial or NaN coordinates
- [x] Tests added for status values, warning text, metadata defaults, and simple-engine compatibility behavior

## Files changed
- `libs/skygate-ephemeris/include/skygate/ephemeris/Types.hpp`
- `libs/skygate-ephemeris/src/engine/simple/SimpleEphemerisEngine.cpp`
- `libs/skygate-ephemeris/tests/engine/EphemerisApiModelTests.cpp`
- `libs/skygate-ephemeris/tests/engine/EphemerisEngineFallbackTests.cpp`
- `.ralph/high-precision-ephemeris-engine/HP-005/implementation.md`

## Important notes
- `cmake --build build-ralph --target skygate-ephemeris-api-model-tests skygate-ephemeris-engine-fallback-tests -j2` passed.
- `ctest --test-dir build-ralph --output-on-failure -R 'skygate-ephemeris-(api-model|engine-fallback)-tests'` passed.
- `ctest --test-dir build-ralph --output-on-failure -R 'skygate-ephemeris-(engine-baseline|engine-fallback|regression)-tests'` passed.
- `cmake --build build-ralph -j2` passed.
- Full `ctest --test-dir build-ralph --output-on-failure` ran 103 tests with 102 passing and the known unrelated `skygate-ui-qml-main-window-tests` failure already tracked as HP-051.
