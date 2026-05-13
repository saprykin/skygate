## Task
- ID: HP-027G
- Title: Add apparent RA/Dec Horizons validation

## Status
READY

## Acceptance criteria claimed
- [x] Geocentric apparent RA/Dec validation fixture added with Horizons source parameters, frame, time scale, target, observer, expected values, and tolerance.
- [x] Apparent RA/Dec validation test target registered.
- [x] Apparent-place precession/nutation output now uses a true-equator/equinox-of-date frame for Horizons apparent RA/Dec semantics.
- [x] Tests pass in the available `build-ralph` configuration.

## Files changed
- `libs/skygate-ephemeris/src/engine/highprecision/ApparentPlaceCalculator.cpp`
- `libs/skygate-ephemeris/src/engine/highprecision/ErfaAstrometry.hpp`
- `libs/skygate-ephemeris/src/engine/highprecision/ErfaAstrometry.cpp`
- `libs/skygate-ephemeris/src/engine/highprecision/FrameTransformer.hpp`
- `libs/skygate-ephemeris/src/engine/highprecision/FrameTransformer.cpp`
- `libs/skygate-ephemeris/tests/CMakeLists.txt`
- `libs/skygate-ephemeris/tests/fixtures/ephemeris/apparent_solar_system_mars_smoke.json`
- `libs/skygate-ephemeris/tests/highprecision/ApparentPlaceCalculatorTests.cpp`
- `libs/skygate-ephemeris/tests/highprecision/ApparentRaDecValidationTests.cpp`

## Important notes
- `ctest --test-dir build-ralph --output-on-failure` passed: 61/61 tests.
- `cmake -S . -B build-ralph -DSKYGATE_ENABLE_HIGH_PRECISION_EPHEMERIS=ON` could not be verified in this environment because `calceph` is not installed.
- The new validation test skips the ERFA-backed numeric assertion in simple-only builds where high-precision dependencies are disabled.
