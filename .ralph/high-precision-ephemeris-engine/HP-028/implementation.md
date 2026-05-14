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

## Review fixes
- Review verdict addressed: NEEDS_FIX
- Findings addressed:
  - Finding title: Required topocentric validation coverage is missing
  - Action: Fixed
  - Notes: Added a deterministic Horizons-derived topocentric observer/apparent fixture covering Moon, Sun, and Mars
    through the real ERFA frame-transform path with fixed UTC/TT and EOP test plumbing. Added focused edge-case tests
    for missing EOP data, parallax enabled versus disabled behavior, and observer elevation effects.
- Files changed during fix pass:
  - `libs/skygate-ephemeris/tests/highprecision/ApparentPlaceCalculatorTests.cpp`
  - `libs/skygate-ephemeris/tests/fixtures/ephemeris/topocentric_observer_apparent_solar_system_smoke.json`
  - `.ralph/high-precision-ephemeris-engine/HP-028/implementation.md`
  - `.ralph/high-precision-ephemeris-engine/HP-028/fix.md`
- Tests run after fix:
  - `cmake --build build-ralph --target skygate-ephemeris-apparent-place-calculator-tests`: PASS
  - `cmake --build build-ralph --target skygate-ephemeris-apparent-place-calculator-tests skygate-ephemeris-solar-system-state-calculator-tests && ctest --test-dir build-ralph -R 'skygate-ephemeris-(apparent-place-calculator|solar-system-state-calculator)-tests' --output-on-failure`: PASS
  - `ctest --test-dir build-ralph --output-on-failure`: PASS
- Remaining concerns: None.
