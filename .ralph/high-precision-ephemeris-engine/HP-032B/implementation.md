## Task
- ID: HP-032B
- Title: Add batch star astrometry propagation

## Status
READY

## Acceptance criteria claimed
- [x] Batch path added for catalog-star astrometry arrays
- [x] Batch results preserve source catalog body indices
- [x] Batch propagation matches single-star propagation
- [x] Full astrometry, partial astrometry, fixed-only stars, and correction
  flags are covered by tests
- [x] Existing tests pass

## Files changed
- `libs/skygate-ephemeris/src/engine/highprecision/StarAstrometryCalculator.hpp`
- `libs/skygate-ephemeris/src/engine/highprecision/StarAstrometryCalculator.cpp`
- `libs/skygate-ephemeris/tests/highprecision/StarAstrometryCalculatorTests.cpp`

## Important notes
- `ctest --test-dir build-ralph --output-on-failure` passed 123/123 tests.
- The configured CALCEPH kernel-provider and solar-system calculator tests
  remain skipped by the existing build configuration.
