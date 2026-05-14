## Task
- ID: HP-032C
- Title: Integrate batch path into full-frame computation

## Status
READY

## Acceptance criteria claimed
- [x] Full-frame high-precision snapshot computation uses the catalog-star
  batch path when available
- [x] Single-object request semantics still use the single-body calculator path
- [x] Non-batched or partial batch results fall back to existing per-body
  dispatch
- [x] Representative catalog coverage verifies one batch call and no per-star
  fallback calls
- [x] Existing tests pass

## Files changed
- `libs/skygate-ephemeris/src/engine/highprecision/HighPrecisionEphemerisEngine.hpp`
- `libs/skygate-ephemeris/src/engine/highprecision/HighPrecisionEphemerisEngine.cpp`
- `libs/skygate-ephemeris/src/engine/highprecision/StarAstrometryCalculator.hpp`
- `libs/skygate-ephemeris/tests/highprecision/HighPrecisionEphemerisEngineTests.cpp`

## Important notes
- Full `ctest --test-dir build-ralph --output-on-failure` passed: 123/123
  configured tests passed, with the existing CALCEPH-disabled tests skipped.

## Review fixes
- Review verdict addressed: NEEDS_FIX
- Findings addressed:
  - Finding title: Real batch path still repeats per-request work per star
  - Action: Fixed
  - Notes: Added request-scoped annual-parallax caching in the real star
    batch calculator. Added batch apparent-place and frame-transform paths so
    full-frame star snapshots reuse frame conversion state, including
    topocentric request-wide time/EOP work and frame-transform matrices.
  - Finding title: Performance acceptance is not verified by a benchmark
  - Action: Fixed
  - Notes: Added representative 4096-star call-count guards proving the real
    integrated batch path uses one frame batch per stage, no single-vector
    frame transforms, and one request-wide topocentric time conversion.
- Files changed during fix pass:
  - `libs/skygate-ephemeris/src/engine/highprecision/ApparentPlaceCalculator.cpp`
  - `libs/skygate-ephemeris/src/engine/highprecision/ApparentPlaceCalculator.hpp`
  - `libs/skygate-ephemeris/src/engine/highprecision/FrameTransformer.cpp`
  - `libs/skygate-ephemeris/src/engine/highprecision/FrameTransformer.hpp`
  - `libs/skygate-ephemeris/src/engine/highprecision/HighPrecisionEphemerisEngine.cpp`
  - `libs/skygate-ephemeris/src/engine/highprecision/HighPrecisionEphemerisEngine.hpp`
  - `libs/skygate-ephemeris/src/engine/highprecision/StarAstrometryCalculator.cpp`
  - `libs/skygate-ephemeris/tests/highprecision/HighPrecisionEphemerisEngineTests.cpp`
  - `libs/skygate-ephemeris/tests/highprecision/StarAstrometryCalculatorTests.cpp`
  - `.ralph/high-precision-ephemeris-engine/HP-032C/fix.md`
  - `.ralph/high-precision-ephemeris-engine/HP-032C/implementation.md`
- Tests run after fix:
  - `cmake --build build-ralph --target
    skygate-ephemeris-star-astrometry-calculator-tests
    skygate-ephemeris-highprecision-engine-tests`
  - `ctest --test-dir build-ralph -R
    "skygate-ephemeris-(star-astrometry-calculator|highprecision-engine)-tests"
    --output-on-failure`
  - `ctest --test-dir build-ralph -R skygate-ephemeris-highprecision
    --output-on-failure`
  - `cmake --build build-ralph --target
    skygate-ephemeris-apparent-place-calculator-tests`
  - `ctest --test-dir build-ralph -R
    skygate-ephemeris-apparent-place-calculator-tests --output-on-failure`
  - `cmake --build build-ralph`
  - `ctest --test-dir build-ralph -R skygate-ephemeris
    --output-on-failure`
  - `ctest --test-dir build-ralph --output-on-failure`
- Remaining concerns: None.
