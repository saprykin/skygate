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

## Review fixes

- Review verdict addressed: NEEDS_FIX
- Findings addressed:
  - Finding title: Batch can mask invalid optional astrometry
  - Action: Fixed
  - Notes: Scalar star astrometry now treats present non-finite optional
    proper-motion and radial-velocity fields with the same usable-value policy
    as catalog astrometry arrays. Requested corrections with unusable optionals
    degrade and report unavailable corrections instead of diverging from batch
    results. Added batch parity coverage for present NaN/Inf optional fields.
  - Finding title: Disabled correction flags are not batch-tested
  - Action: Fixed
  - Notes: Added a batch parity test using
    `EphemerisCorrectionFlags::NoCorrections` for full-astrometry,
    partial-astrometry, and fixed-only stars.
- Files changed during fix pass:
  - `StarAstrometryCalculator.cpp`
    in `libs/skygate-ephemeris/src/engine/highprecision/`
  - `StarAstrometryCalculatorTests.cpp`
    in `libs/skygate-ephemeris/tests/highprecision/`
  - `.ralph/high-precision-ephemeris-engine/HP-032B/implementation.md`
  - `.ralph/high-precision-ephemeris-engine/HP-032B/fix.md`
- Tests run after fix:
  - Command: `cmake --build build-ralph --target`
    `skygate-ephemeris-star-astrometry-calculator-tests`
    Result: PASS
  - Command: `ctest --test-dir build-ralph --output-on-failure -R`
    `skygate-ephemeris-star-astrometry-calculator-tests`
    Result: PASS
  - `git diff --check` PASS
  - `ctest --test-dir build-ralph --output-on-failure` PASS
- Remaining concerns: None.
