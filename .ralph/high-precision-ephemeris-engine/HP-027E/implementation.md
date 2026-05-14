## Task
- ID: HP-027E
- Title: Implement annual parallax handling for apparent-place inputs

## Status
READY

## Acceptance criteria claimed
- [x] Annual parallax is applied when catalog parallax metadata and Earth
  barycentric state are available
- [x] Annual parallax degrades with warning metadata when source parallax
  data is missing
- [x] Annual parallax degrades with warning metadata when kernel data wiring is
  unavailable
- [x] Factory wiring passes the CALCEPH kernel provider to star astrometry
- [x] Focused annual-parallax tests added
- [x] Existing tests pass

## Files changed
- `libs/skygate-ephemeris/src/engine/highprecision/StarAstrometryCalculator.hpp`
- `libs/skygate-ephemeris/src/engine/highprecision/StarAstrometryCalculator.cpp`
- `libs/skygate-ephemeris/src/engine/simple/SimpleEphemerisEngine.cpp`
- `libs/skygate-ephemeris/tests/highprecision/StarAstrometryCalculatorTests.cpp`

## Important notes
- Full verification passed with `ctest --test-dir build-ralph
  --output-on-failure`: 122/122 tests passed.
- The existing build configuration skipped
  `skygate-ephemeris-calceph-kernel-provider-tests` and
  `skygate-ephemeris-solar-system-state-calculator-tests`.

## Review fixes
- Review verdict addressed: NEEDS_FIX
- Findings addressed:
  - Annual parallax uses non-TDB epochs with CALCEPH
    - Action: Fixed
    - Notes: `StarAstrometryCalculator` now converts annual-parallax kernel
      epochs to TDB through the wired `ITimeScaleService` before calling the
      CALCEPH kernel provider. Missing or failed conversion degrades only the
      annual-parallax correction and records warning metadata.
- Files changed during fix pass:
  - `libs/skygate-ephemeris/src/engine/highprecision/StarAstrometryCalculator.hpp`
  - `libs/skygate-ephemeris/src/engine/highprecision/StarAstrometryCalculator.cpp`
  - `libs/skygate-ephemeris/src/engine/simple/SimpleEphemerisEngine.cpp`
  - `libs/skygate-ephemeris/tests/highprecision/StarAstrometryCalculatorTests.cpp`
  - `.ralph/high-precision-ephemeris-engine/HP-027E/implementation.md`
  - `.ralph/high-precision-ephemeris-engine/HP-027E/fix.md`
- Tests run after fix:
  - `cmake --build build-ralph --target
    skygate-ephemeris-star-astrometry-calculator-tests`: PASS
  - `ctest --test-dir build-ralph -R
    skygate-ephemeris-star-astrometry-calculator-tests
    --output-on-failure`: PASS
  - `ctest --test-dir build-ralph --output-on-failure`: PASS
- Remaining concerns: None.
