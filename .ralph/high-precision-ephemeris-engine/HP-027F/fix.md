## Task fixed
  - ID: HP-027F
  - Title: Record applied correction flags, warnings, and per-flag tests
  - Source: IMPLEMENTATION_PLAN.md HP-027F

## Review input
  - Review verdict: NEEDS_FIX
  - Review report:
    `.ralph/high-precision-ephemeris-engine/HP-027F/review.md`
  - Implementation handoff:
    `.ralph/high-precision-ephemeris-engine/HP-027F/implementation.md`

## Summary
Fixed three correction metadata edge cases found by review: invalid observers
now emit stable unavailable-correction warnings, partial proper-motion source
data no longer reports the same flag as both applied and unavailable, and
simple-engine refraction options no longer degrade `NoCorrections` requests.

## Findings addressed
  - Finding title: Missing observer omits correction warning
  - Severity: MAJOR
  - Action: Fixed
  - File(s):
    `libs/skygate-ephemeris/src/engine/highprecision/ApparentPlaceCalculator.cpp`,
    `libs/skygate-ephemeris/tests/highprecision/ApparentPlaceCalculatorTests.cpp`
  - What changed: The invalid-observer topocentric branch now records
    `DiurnalParallax` with `addUnavailableCorrection()` and the regression test
    asserts both `MissingObserver` and `CorrectionUnavailable`.
  - Why this resolves the finding: Requested-but-unavailable diurnal parallax
    now sets both the unavailable flag and stable warning metadata.

  - Finding title: Partial proper motion is both applied and unavailable
  - Severity: MAJOR
  - Action: Fixed
  - File(s):
    `libs/skygate-ephemeris/src/engine/highprecision/StarAstrometryCalculator.cpp`,
    `libs/skygate-ephemeris/tests/highprecision/StarAstrometryCalculatorTests.cpp`
  - What changed: `ProperMotion` is marked applied only when both proper-motion
    components are present. The partial-data test now asserts unavailable and
    not applied.
  - Why this resolves the finding: A single correction flag can no longer land
    in both applied and unavailable metadata buckets for partial proper motion.

  - Finding title: Refraction-only simple requests lose unavailable flag detail
  - Severity: MAJOR
  - Action: Fixed
  - File(s):
    `libs/skygate-ephemeris/src/engine/simple/SimpleEphemerisEngine.cpp`,
    `libs/skygate-ephemeris/tests/engine/EphemerisEngineBaselineTests.cpp`
  - What changed: The simple engine now treats the refraction enable option as
    active only when a correction flag requests it. A regression test covers
    `enableAtmosphericRefraction=true` with `NoCorrections`.
  - Why this resolves the finding: `NoCorrections` requests no longer produce
    a correction-unavailable warning with no corresponding requested,
    skipped, or unavailable correction bit.

## Tests run
  - `cmake --build build-ralph --target
    skygate-ephemeris-apparent-place-calculator-tests
    skygate-ephemeris-star-astrometry-calculator-tests
    skygate-ephemeris-engine-baseline-tests`: PASS
  - `ctest --test-dir build-ralph --output-on-failure -R
    'apparent-place|star-astrometry|engine-baseline'`: PASS
  - `ctest --test-dir build-ralph --output-on-failure`: PASS; 120 passed,
    2 CALCEPH-gated tests skipped.

## Files changed
  - `.ralph/high-precision-ephemeris-engine/HP-027F/implementation.md`
  - `.ralph/high-precision-ephemeris-engine/HP-027F/fix.md`
  - `libs/skygate-ephemeris/src/engine/highprecision/ApparentPlaceCalculator.cpp`
  - `libs/skygate-ephemeris/src/engine/highprecision/StarAstrometryCalculator.cpp`
  - `libs/skygate-ephemeris/src/engine/simple/SimpleEphemerisEngine.cpp`
  - `libs/skygate-ephemeris/tests/highprecision/ApparentPlaceCalculatorTests.cpp`
  - `libs/skygate-ephemeris/tests/highprecision/StarAstrometryCalculatorTests.cpp`
  - `libs/skygate-ephemeris/tests/engine/EphemerisEngineBaselineTests.cpp`

## Remaining concerns
None.

## Final fixer status
READY_FOR_REVIEW
