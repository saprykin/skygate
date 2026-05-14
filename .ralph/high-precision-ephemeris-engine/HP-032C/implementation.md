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
