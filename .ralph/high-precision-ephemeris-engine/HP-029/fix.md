## Task fixed
  - ID: HP-029
  - Title: Implement atmospheric refraction calculation
  - Source: `IMPLEMENTATION_PLAN.md` HP-029

## Review input
  - Review verdict: NEEDS_FIX
  - Review report: `.ralph/high-precision-ephemeris-engine/HP-029/review.md`
  - Implementation handoff:
    `.ralph/high-precision-ephemeris-engine/HP-029/implementation.md`

## Summary
  Added the missing atmospheric-refraction validation tests required by the
  review. Invalid observer and atmosphere inputs now have focused coverage that
  verifies refraction is not applied, altitude is unchanged, metadata is
  degraded, and `CorrectionUnavailable` is reported.

## Findings addressed
  - Finding title: Refraction input validation coverage is incomplete
  - Severity: MAJOR
  - Action: Fixed
  - File(s):
    `libs/skygate-ephemeris/tests/highprecision/AtmosphericRefractionCalculatorTests.cpp`
  - What changed: Added invalid-observer coverage, data-driven invalid
    atmosphere coverage for pressure, temperature, relative humidity, and
    wavelength validation branches, and extra altitude boundary coverage.
  - Why this resolves the finding: The reviewed availability contract is now
    directly asserted for invalid observer and atmosphere requests, including
    unchanged altitude, degraded status, `CorrectionUnavailable`, and no applied
    atmospheric-refraction flag.

## Tests run
  - `clang-format -i
    libs/skygate-ephemeris/tests/highprecision/AtmosphericRefractionCalculatorTests.cpp`:
    PASS
  - `cmake --build build-ralph --target
    skygate-ephemeris-atmospheric-refraction-calculator-tests
    skygate-ephemeris-apparent-place-calculator-tests
    skygate-ephemeris-highprecision-engine-tests
    skygate-ephemeris-apparent-radec-validation-tests`: PASS
  - `ctest --test-dir build-ralph --output-on-failure -R
    '^(skygate-ephemeris-atmospheric-refraction-calculator-tests|skygate-ephemeris-apparent-place-calculator-tests|skygate-ephemeris-highprecision-engine-tests|skygate-ephemeris-apparent-radec-validation-tests)$'`:
    PASS
  - `ctest --test-dir build-ralph --output-on-failure`: PASS, 121/121 tests
    passed with CALCEPH-gated tests skipped in this build configuration

## Files changed
  - `.ralph/high-precision-ephemeris-engine/HP-029/fix.md`
  - `.ralph/high-precision-ephemeris-engine/HP-029/implementation.md`
  - `libs/skygate-ephemeris/tests/highprecision/AtmosphericRefractionCalculatorTests.cpp`

## Remaining concerns
  None.

## Final fixer status
  READY_FOR_REVIEW
