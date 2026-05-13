## Task
- ID: HP-026A
- Title: Implement celestial frame transforms

## Status
READY

## Acceptance criteria claimed
- [x] ERFA/SOFA wrapper exposes the IAU 2006/2000A celestial-to-intermediate matrix.
- [x] `FrameTransformer` boundary added for ICRS, GCRS, and CIRS celestial vector transforms.
- [x] Non-TT epochs use the public time-scale service before CIRS transforms.
- [x] Tests added for SOFA reference values, ICRS/GCRS identity behavior, time-scale-service routing, and GCRS/CIRS round trip precision.
- [x] Existing tests pass in the available `build-ralph` configuration.

## Files changed
- `libs/skygate-ephemeris/CMakeLists.txt`
- `libs/skygate-ephemeris/src/engine/highprecision/ErfaAstrometry.hpp`
- `libs/skygate-ephemeris/src/engine/highprecision/ErfaAstrometry.cpp`
- `libs/skygate-ephemeris/src/engine/highprecision/FrameTransformer.hpp`
- `libs/skygate-ephemeris/src/engine/highprecision/FrameTransformer.cpp`
- `libs/skygate-ephemeris/src/engine/highprecision/HighPrecisionEphemerisEngine.hpp`
- `libs/skygate-ephemeris/tests/CMakeLists.txt`
- `libs/skygate-ephemeris/tests/highprecision/FrameTransformerTests.cpp`

## Important notes
- `ctest --test-dir build-ralph --output-on-failure` passed 58/58 tests with `SKYGATE_ENABLE_HIGH_PRECISION_EPHEMERIS=OFF`.
- Reconfiguring `build-ralph` with `SKYGATE_ENABLE_HIGH_PRECISION_EPHEMERIS=ON` is blocked in this container because CMake cannot find `calceph`; therefore the new ERFA-gated `skygate-ephemeris-frame-transformer-tests` target could not be executed here.
