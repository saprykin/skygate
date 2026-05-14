## Task fixed
  - ID: HP-032C
  - Title: Integrate batch path into full-frame computation
  - Source: IMPLEMENTATION_PLAN.md

## Review input
  - Review verdict: NEEDS_FIX
  - Review report:
    `.ralph/high-precision-ephemeris-engine/HP-032C/review.md`
  - Implementation handoff:
    `.ralph/high-precision-ephemeris-engine/HP-032C/implementation.md`

## Summary
  Fixed the remaining batch-performance gaps by caching annual-parallax
  request state in the real star batch calculator and batching apparent-place
  frame transforms for full-frame star snapshots. Added representative
  4096-star call-count guards for both apparent and topocentric paths.

## Findings addressed
  - Finding title: Real batch path still repeats per-request work per star
  - Severity: MAJOR
  - Action: Fixed
  - File(s):
    `libs/skygate-ephemeris/src/engine/highprecision/StarAstrometryCalculator.cpp`,
    `libs/skygate-ephemeris/src/engine/highprecision/ApparentPlaceCalculator.cpp`,
    `libs/skygate-ephemeris/src/engine/highprecision/ApparentPlaceCalculator.hpp`,
    `libs/skygate-ephemeris/src/engine/highprecision/FrameTransformer.cpp`,
    `libs/skygate-ephemeris/src/engine/highprecision/FrameTransformer.hpp`,
    `libs/skygate-ephemeris/src/engine/highprecision/HighPrecisionEphemerisEngine.cpp`,
    `libs/skygate-ephemeris/src/engine/highprecision/HighPrecisionEphemerisEngine.hpp`
  - What changed: `StarAstrometryCalculator::calculateBatch()` now shares a
    request cache for TDB conversion and Earth barycentric kernel state.
    Full-frame star snapshots now call batch apparent-place processing.
    ERFA frame transforms expose a batch API that reuses conversion, EOP, and
    matrix state across vectors in one request.
  - Why this resolves the finding: The repeated per-star request-wide work is
    computed once for the batch request, while single-object APIs still use the
    existing single-body path.

  - Finding title: Performance acceptance is not verified by a benchmark
  - Severity: MAJOR
  - Action: Fixed
  - File(s):
    `libs/skygate-ephemeris/tests/highprecision/StarAstrometryCalculatorTests.cpp`,
    `libs/skygate-ephemeris/tests/highprecision/HighPrecisionEphemerisEngineTests.cpp`
  - What changed: Added call-count assertions proving annual-parallax batch
    work performs one time conversion and one Earth kernel lookup across
    multiple stars. Added 4096-star full-frame guards for apparent and
    topocentric batch paths.
  - Why this resolves the finding: The tests now fail if request-wide work
    regresses to per-star time/kernel/frame collaborators in the integrated
    batch path.

## Tests run
  - Command: `cmake --build build-ralph --target
    skygate-ephemeris-star-astrometry-calculator-tests
    skygate-ephemeris-highprecision-engine-tests`
    Result: PASS
  - Command: `ctest --test-dir build-ralph -R
    "skygate-ephemeris-(star-astrometry-calculator|highprecision-engine)-tests"
    --output-on-failure`
    Result: PASS
  - Command: `ctest --test-dir build-ralph -R
    skygate-ephemeris-highprecision --output-on-failure`
    Result: PASS
  - Command: `cmake --build build-ralph --target
    skygate-ephemeris-apparent-place-calculator-tests`
    Result: PASS
  - Command: `ctest --test-dir build-ralph -R
    skygate-ephemeris-apparent-place-calculator-tests
    --output-on-failure`
    Result: PASS
  - Command: `cmake --build build-ralph`
    Result: PASS
  - Command: `ctest --test-dir build-ralph -R skygate-ephemeris
    --output-on-failure`
    Result: PASS
  - Command: `ctest --test-dir build-ralph --output-on-failure`
    Result: PASS

## Files changed
  - `.ralph/high-precision-ephemeris-engine/HP-032C/fix.md`
  - `.ralph/high-precision-ephemeris-engine/HP-032C/implementation.md`
  - `libs/skygate-ephemeris/src/engine/highprecision/ApparentPlaceCalculator.cpp`
  - `libs/skygate-ephemeris/src/engine/highprecision/ApparentPlaceCalculator.hpp`
  - `libs/skygate-ephemeris/src/engine/highprecision/FrameTransformer.cpp`
  - `libs/skygate-ephemeris/src/engine/highprecision/FrameTransformer.hpp`
  - `libs/skygate-ephemeris/src/engine/highprecision/HighPrecisionEphemerisEngine.cpp`
  - `libs/skygate-ephemeris/src/engine/highprecision/HighPrecisionEphemerisEngine.hpp`
  - `libs/skygate-ephemeris/src/engine/highprecision/StarAstrometryCalculator.cpp`
  - `libs/skygate-ephemeris/tests/highprecision/HighPrecisionEphemerisEngineTests.cpp`
  - `libs/skygate-ephemeris/tests/highprecision/StarAstrometryCalculatorTests.cpp`

## Remaining concerns
  None.

## Final fixer status
  READY_FOR_REVIEW
