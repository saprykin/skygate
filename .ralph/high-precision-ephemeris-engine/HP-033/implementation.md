## Task
- ID: HP-033
- Title: Add high-precision computation cache and read-only thread safety

## Status
READY

## Acceptance criteria claimed
- [x] Computation cache added for repeated full-frame high-precision requests
- [x] Factory wires high-precision engines with a default computation cache
- [x] Cached results are isolated by request, catalog, and data-set revision
- [x] Concurrent read-only compute coverage added
- [x] Existing tests pass

## Files changed
- `libs/skygate-ephemeris/CMakeLists.txt`
- `libs/skygate-ephemeris/src/engine/highprecision/EphemerisComputationCache.hpp`
- `libs/skygate-ephemeris/src/engine/highprecision/EphemerisComputationCache.cpp`
- `libs/skygate-ephemeris/src/engine/highprecision/HighPrecisionEphemerisEngine.hpp`
- `libs/skygate-ephemeris/src/engine/highprecision/HighPrecisionEphemerisEngine.cpp`
- `libs/skygate-ephemeris/src/engine/simple/SimpleEphemerisEngine.cpp`
- `libs/skygate-ephemeris/tests/highprecision/HighPrecisionEphemerisEngineTests.cpp`

## Important notes
- Verification passed with `ctest --test-dir build-ralph --output-on-failure`.
- CALCEPH kernel smoke tests were skipped by their existing test conditions.
