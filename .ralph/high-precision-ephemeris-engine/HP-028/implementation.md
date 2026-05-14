## Task
- ID: HP-028
- Title: Implement topocentric observer and diurnal parallax pipeline

## Status
READY

## Acceptance criteria claimed
- [x] Solar-system calculator preserves observer-relative distance vectors for downstream topocentric correction
- [x] Apparent-place pipeline computes WGS84 observer ITRS position and applies diurnal/topocentric parallax when requested
- [x] Topocentric requests produce horizontal coordinates from the corrected topocentric ITRS vector
- [x] Invalid observer and unavailable distance-vector cases return degraded warning metadata instead of silently applying parallax
- [x] Focused topocentric and solar-system vector tests added
- [x] Existing tests pass

## Files changed
- `libs/skygate-ephemeris/src/engine/highprecision/HighPrecisionEphemerisEngine.hpp`
- `libs/skygate-ephemeris/src/engine/highprecision/SolarSystemStateCalculator.cpp`
- `libs/skygate-ephemeris/src/engine/highprecision/ApparentPlaceCalculator.cpp`
- `libs/skygate-ephemeris/tests/highprecision/ApparentPlaceCalculatorTests.cpp`
- `libs/skygate-ephemeris/tests/highprecision/SolarSystemStateCalculatorTests.cpp`

## Important notes
- Full `ctest --test-dir build-ralph --output-on-failure` passed with 61/61 tests.
