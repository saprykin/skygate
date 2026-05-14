## Task
- ID: HP-027F
- Title: Record applied correction flags, warnings, and per-flag tests

## Status
READY

## Acceptance criteria claimed
- [x] Result metadata records requested correction flags.
- [x] Result metadata records applied correction flags.
- [x] Result metadata distinguishes skipped and unavailable correction flags.
- [x] Requested-but-unavailable corrections set stable warning metadata.
- [x] Tests cover applied, skipped, and unavailable correction metadata.
- [x] Existing configured tests pass.

## Files changed
- `libs/skygate-ephemeris/include/skygate/ephemeris/Types.hpp`
- `libs/skygate-ephemeris/src/engine/simple/SimpleEphemerisEngine.cpp`
- `libs/skygate-ephemeris/src/engine/highprecision/ApparentPlaceCalculator.cpp`
- `libs/skygate-ephemeris/src/engine/highprecision/AtmosphericRefractionCalculator.cpp`
- `libs/skygate-ephemeris/src/engine/highprecision/EphemerisResultBuilder.cpp`
- `libs/skygate-ephemeris/src/engine/highprecision/SolarSystemStateCalculator.cpp`
- `libs/skygate-ephemeris/src/engine/highprecision/StarAstrometryCalculator.cpp`
- `libs/skygate-ephemeris/tests/highprecision/ApparentPlaceCalculatorTests.cpp`
- `libs/skygate-ephemeris/tests/highprecision/AtmosphericRefractionCalculatorTests.cpp`
- `libs/skygate-ephemeris/tests/highprecision/HighPrecisionEphemerisEngineTests.cpp`
- `libs/skygate-ephemeris/tests/highprecision/SolarSystemStateCalculatorTests.cpp`
- `libs/skygate-ephemeris/tests/highprecision/StarAstrometryCalculatorTests.cpp`
- `libs/skygate-ephemeris/tests/engine/EphemerisApiModelTests.cpp`

## Important notes
- `build-ralph` is configured with
  `SKYGATE_ENABLE_HIGH_PRECISION_EPHEMERIS=OFF`, so CALCEPH-gated tests were
  reported by CTest as skipped.
- Fixed a related apparent-place metadata issue where GCRS pass-through
  requests could be degraded by a missing frame transformer even though no
  frame transform was needed.

## Review fixes

- Review verdict addressed: NEEDS_FIX
- Findings addressed:
  - Missing observer omits correction warning
    - Action: Fixed
    - Notes: Invalid topocentric observers now use
      `addUnavailableCorrection(DiurnalParallax)` so both `MissingObserver`
      and `CorrectionUnavailable` are recorded.
  - Partial proper motion is both applied and unavailable
    - Action: Fixed
    - Notes: `ProperMotion` is recorded as applied only when both right
      ascension and declination components are available. Partial source data
      remains degraded and unavailable.
  - Refraction-only simple requests lose unavailable flag detail
    - Action: Fixed
    - Notes: Simple-engine requests with `NoCorrections` no longer treat the
      refraction enable option alone as a requested unsupported correction.
- Files changed during fix pass:
  - `libs/skygate-ephemeris/src/engine/highprecision/ApparentPlaceCalculator.cpp`
  - `libs/skygate-ephemeris/src/engine/highprecision/StarAstrometryCalculator.cpp`
  - `libs/skygate-ephemeris/src/engine/simple/SimpleEphemerisEngine.cpp`
  - `libs/skygate-ephemeris/tests/highprecision/ApparentPlaceCalculatorTests.cpp`
  - `libs/skygate-ephemeris/tests/highprecision/StarAstrometryCalculatorTests.cpp`
  - `libs/skygate-ephemeris/tests/engine/EphemerisEngineBaselineTests.cpp`
- Tests run after fix:
  - `cmake --build build-ralph --target
    skygate-ephemeris-apparent-place-calculator-tests
    skygate-ephemeris-star-astrometry-calculator-tests
    skygate-ephemeris-engine-baseline-tests`: PASS
  - `ctest --test-dir build-ralph --output-on-failure -R
    'apparent-place|star-astrometry|engine-baseline'`: PASS
  - `ctest --test-dir build-ralph --output-on-failure`: PASS; 120 passed,
    2 CALCEPH-gated tests skipped.
- Remaining concerns: None.
