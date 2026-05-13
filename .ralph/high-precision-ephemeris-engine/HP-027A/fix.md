## Task fixed
  - ID: HP-027A
  - Title: Add `ApparentPlaceCalculator` boundary and request mode routing
  - Source: `IMPLEMENTATION_PLAN.md`

## Review input
  - Review verdict: NEEDS_FIX
  - Review report: `.ralph/high-precision-ephemeris-engine/HP-027A/review.md`
  - Implementation handoff: `.ralph/high-precision-ephemeris-engine/HP-027A/implementation.md`

## Summary
  Fixed astrometric request-mode classification so unsupported extra correction flags do not convert an astrometric request into the apparent/CIRS frame path. Added regression coverage for astrometric plus atmospheric refraction and direct calculator-level geometric routing.

## Findings addressed
  - Finding title: Astrometric requests with extra unsupported flags route as apparent
  - Severity: MAJOR
  - Action: Fixed
  - File(s): `libs/skygate-ephemeris/src/engine/highprecision/ApparentPlaceCalculator.cpp`, `libs/skygate-ephemeris/tests/highprecision/ApparentPlaceCalculatorTests.cpp`
  - What changed: `requestModeForCorrections()` now routes `DiurnalParallax` to topocentric, `EarthOrientation` to apparent, and other geocentric correction combinations to astrometric. Tests now cover geometric/GCRS routing and `Astrometric | AtmosphericRefraction` staying on GCRS while reporting degraded correction-unavailable metadata.
  - Why this resolves the finding: Unsupported refraction no longer changes the requested astrometric frame route; it is reported through structured degraded metadata after the correct GCRS transform path is selected.

## Tests run
  - `cmake --build build-ralph --target skygate-ephemeris-apparent-place-calculator-tests skygate-ephemeris-highprecision-engine-tests` - PASS
  - `ctest --test-dir build-ralph --output-on-failure -R "skygate-ephemeris-(highprecision-engine|apparent-place-calculator)-tests"` - PASS
  - `ctest --test-dir build-ralph --output-on-failure` - PASS

## Files changed
  - `libs/skygate-ephemeris/src/engine/highprecision/ApparentPlaceCalculator.cpp`
  - `libs/skygate-ephemeris/tests/highprecision/ApparentPlaceCalculatorTests.cpp`
  - `.ralph/high-precision-ephemeris-engine/HP-027A/implementation.md`
  - `.ralph/high-precision-ephemeris-engine/HP-027A/fix.md`

## Remaining concerns
  None.

## Final fixer status
  READY_FOR_REVIEW
