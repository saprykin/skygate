## Task fixed
  - ID: HP-032B
  - Title: Add batch star astrometry propagation
  - Source: IMPLEMENTATION_PLAN.md / specs/high-precision-ephemeris-engine.md

## Review input
  - Review verdict: NEEDS_FIX
  - Review report: .ralph/high-precision-ephemeris-engine/HP-032B/review.md
  - Implementation handoff:
    .ralph/high-precision-ephemeris-engine/HP-032B/implementation.md

## Summary
  Fixed the batch/scalar mismatch for present non-finite optional star
  astrometry and added the missing disabled-correction batch parity coverage.

## Findings addressed
  - Finding title: Batch can mask invalid optional astrometry
  - Severity: MAJOR
  - Action: Fixed
  - File(s):
    libs/skygate-ephemeris/src/engine/highprecision/
    StarAstrometryCalculator.cpp,
    libs/skygate-ephemeris/tests/highprecision/StarAstrometryCalculatorTests.cpp
  - What changed: Scalar star astrometry now uses finite optional values only
    when applying and reporting proper-motion and radial-velocity corrections.
    Present non-finite optional values are treated as unavailable correction
    inputs, matching the catalog astrometry array fallback semantics. Added
    batch parity coverage for NaN/Inf optional astrometry fields.
  - Why this resolves the finding: Batch reconstruction from sanitized arrays
    and scalar computation now produce the same degraded result metadata and
    coordinates for unusable optional astrometry.

  - Finding title: Disabled correction flags are not batch-tested
  - Severity: MINOR
  - Action: Fixed
  - File(s):
    libs/skygate-ephemeris/tests/highprecision/StarAstrometryCalculatorTests.cpp
  - What changed: Added a `NoCorrections` batch parity test covering full
    astrometry, partial astrometry, and fixed-only stars.
  - Why this resolves the finding: Batch results are now checked against
    single-star outputs when all astrometry corrections are disabled.

## Tests run
  - Command: `cmake --build build-ralph --target`
    `skygate-ephemeris-star-astrometry-calculator-tests`
    Result: PASS
  - Command: `ctest --test-dir build-ralph --output-on-failure -R`
    `skygate-ephemeris-star-astrometry-calculator-tests`
    Result: PASS
  - `git diff --check` PASS
  - `ctest --test-dir build-ralph --output-on-failure` PASS

## Files changed
  - libs/skygate-ephemeris/src/engine/highprecision/StarAstrometryCalculator.cpp
  - libs/skygate-ephemeris/tests/highprecision/StarAstrometryCalculatorTests.cpp
  - .ralph/high-precision-ephemeris-engine/HP-032B/implementation.md
  - .ralph/high-precision-ephemeris-engine/HP-032B/fix.md

## Remaining concerns
  None.

## Final fixer status
  READY_FOR_REVIEW
