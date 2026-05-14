## Task fixed
  - ID: HP-027E
  - Title: Implement annual parallax handling for apparent-place inputs
  - Source: IMPLEMENTATION_PLAN.md

## Review input
  - Review verdict: NEEDS_FIX
  - Review report:
    .ralph/high-precision-ephemeris-engine/HP-027E/review.md
  - Implementation handoff:
    .ralph/high-precision-ephemeris-engine/HP-027E/implementation.md

## Summary
  Annual-parallax Earth-state lookups now convert the request epoch to TDB
  before calling the CALCEPH kernel provider. The high-precision factory wires
  the existing time-scale service into the star astrometry calculator, and the
  focused test now enforces the real provider's TDB-only contract.

## Findings addressed
  - Finding title: Annual parallax uses non-TDB epochs with CALCEPH
  - Severity: MAJOR
  - Action: Fixed
  - File(s):
    `libs/skygate-ephemeris/src/engine/highprecision/StarAstrometryCalculator.hpp`,
    `libs/skygate-ephemeris/src/engine/highprecision/StarAstrometryCalculator.cpp`,
    `libs/skygate-ephemeris/src/engine/simple/SimpleEphemerisEngine.cpp`,
    `libs/skygate-ephemeris/tests/highprecision/StarAstrometryCalculatorTests.cpp`
  - What changed: Added time-scale service wiring to
    `StarAstrometryCalculator`, converted annual-parallax kernel epochs to TDB,
    and covered the path with a TDB-enforcing fake kernel provider.
  - Why this resolves the finding: CALCEPH-backed Earth barycentric lookups now
    receive a TDB epoch for normal TT or UTC requests when conversion data is
    available. If conversion is unavailable, the result degrades with warning
    metadata instead of silently reporting annual parallax as applied.

## Tests run
  - `cmake --build build-ralph --target
    skygate-ephemeris-star-astrometry-calculator-tests`: PASS
  - `ctest --test-dir build-ralph -R
    skygate-ephemeris-star-astrometry-calculator-tests --output-on-failure`:
    PASS
  - `ctest --test-dir build-ralph --output-on-failure`: PASS

## Files changed
  - `.ralph/high-precision-ephemeris-engine/HP-027E/implementation.md`
  - `.ralph/high-precision-ephemeris-engine/HP-027E/fix.md`
  - `libs/skygate-ephemeris/src/engine/highprecision/StarAstrometryCalculator.hpp`
  - `libs/skygate-ephemeris/src/engine/highprecision/StarAstrometryCalculator.cpp`
  - `libs/skygate-ephemeris/src/engine/simple/SimpleEphemerisEngine.cpp`
  - `libs/skygate-ephemeris/tests/highprecision/StarAstrometryCalculatorTests.cpp`

## Remaining concerns
  None.

## Final fixer status
  READY_FOR_REVIEW
