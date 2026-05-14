## Task
- ID: HP-037
- Title: Add apparent/topocentric and correction validation targets

## Status
READY

## Acceptance criteria claimed
- [x] Apparent RA/Dec validation target is registered.
- [x] Topocentric Moon/Sun/planet validation is registered with validation
  labels.
- [x] Correction flag behavior is registered with validation labels.
- [x] Atmospheric refraction validation target is registered.
- [x] Horizons observer/apparent fixture metadata is asserted by the
  topocentric validation test.
- [x] Existing tests pass.

## Files changed
- `libs/skygate-ephemeris/tests/CMakeLists.txt`
- `libs/skygate-ephemeris/tests/highprecision/ApparentPlaceCalculatorTests.cpp`
- `.ralph/high-precision-ephemeris-engine/HP-037/implementation.md`

## Important notes
- `ctest --test-dir build-ralph --output-on-failure` passed 121/121 tests.
- The existing disabled-high-precision build skipped the two CALCEPH-dependent
  validation tests as configured.
