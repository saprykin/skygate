## Task
- ID: HP-030
- Title: Implement high-precision result assembly

## Status
READY

## Acceptance criteria claimed
- [x] `EphemerisResultBuilder` added as the high-precision result assembly component
- [x] Calculator outputs preserve provenance, validity range, uncertainty, warnings, and applied correction flags
- [x] Out-of-range results with fallback coordinates are returned as degraded results with warnings
- [x] Out-of-range and failed results without fallback coordinates remain explicit
- [x] Tests added for valid, degraded, out-of-range, and failed result assembly behavior
- [x] Existing tests pass

## Files changed
- `libs/skygate-ephemeris/CMakeLists.txt`
- `libs/skygate-ephemeris/src/engine/highprecision/EphemerisResultBuilder.hpp`
- `libs/skygate-ephemeris/src/engine/highprecision/EphemerisResultBuilder.cpp`
- `libs/skygate-ephemeris/src/engine/highprecision/HighPrecisionEphemerisEngine.cpp`
- `libs/skygate-ephemeris/tests/highprecision/HighPrecisionEphemerisEngineTests.cpp`

## Important notes
- Verified with `cmake --build build-ralph -j2`.
- Verified with `ctest --test-dir build-ralph --output-on-failure`.
