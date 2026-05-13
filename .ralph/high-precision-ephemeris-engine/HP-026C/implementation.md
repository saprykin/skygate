## Task
- ID: HP-026C
- Title: Add `FrameTransformer` orchestration and per-stage metadata

## Status
READY

## Acceptance criteria claimed
- [x] Frame transform results expose per-stage metadata for composed transforms
- [x] Multi-stage orchestration reuses cached time-scale conversions and Earth-orientation sampling within a request
- [x] Aggregate metadata preserves per-stage status, warning, provenance, and applied-correction information
- [x] Tests added for composed-stage metadata, cached conversion reuse, and unavailable-stage metadata
- [x] Existing configured tests pass

## Files changed
- `libs/skygate-ephemeris/src/engine/highprecision/FrameTransformer.hpp`
- `libs/skygate-ephemeris/src/engine/highprecision/FrameTransformer.cpp`
- `libs/skygate-ephemeris/tests/highprecision/FrameTransformerTests.cpp`

## Important notes
- `cmake -S . -B build-ralph -DSKYGATE_BUILD_UI=OFF -DSKYGATE_BUILD_TESTS=ON -DSKYGATE_ENABLE_HIGH_PRECISION_EPHEMERIS=ON` could not configure in this environment because `calceph` is not installed.
- Reconfigured `build-ralph` with `SKYGATE_ENABLE_HIGH_PRECISION_EPHEMERIS=OFF` and verified the available suite with `ctest --test-dir build-ralph --output-on-failure`: 58/58 tests passed.

## Review fixes
- Review verdict addressed: NEEDS_FIX
- Findings addressed:
  - Finding title: Composed terrestrial transforms still duplicate EOP lookup work
    - Action: Fixed
    - Notes: `FrameTransformContext` now derives UT1 from the request-scoped Earth-orientation sample when an EOP provider is available, so the CIRS/TIRS and TIRS/ITRS stages share one sampled EOP result. Added a recording-provider test using `LeapSecondTimeScaleService` to prove the composed GCRS-to-ITRS path does not route through both UT1 conversion EOP sampling and polar-motion EOP sampling.
  - Finding title: ICRS requests are reported as GCRS stages
    - Action: Fixed
    - Notes: Stage metadata now preserves the actual request boundary frame on the first forward stage and last reverse stage. Added ICRS-to-ITRS and ITRS-to-ICRS metadata coverage.
- Files changed during fix pass:
  - `libs/skygate-ephemeris/src/engine/highprecision/FrameTransformer.cpp`
  - `libs/skygate-ephemeris/tests/highprecision/FrameTransformerTests.cpp`
  - `.ralph/high-precision-ephemeris-engine/HP-026C/implementation.md`
  - `.ralph/high-precision-ephemeris-engine/HP-026C/fix.md`
- Tests run after fix:
  - `clang-format -i libs/skygate-ephemeris/src/engine/highprecision/FrameTransformer.cpp libs/skygate-ephemeris/tests/highprecision/FrameTransformerTests.cpp`: PASS
  - `cmake -S . -B build-ralph -DSKYGATE_BUILD_UI=OFF -DSKYGATE_BUILD_TESTS=ON -DSKYGATE_ENABLE_HIGH_PRECISION_EPHEMERIS=OFF`: PASS
  - `cmake --build build-ralph --target skygate-ephemeris --parallel`: PASS
  - `ctest --test-dir build-ralph --output-on-failure`: PASS, 58/58 tests
  - `cmake -S . -B build-ralph -DSKYGATE_BUILD_UI=OFF -DSKYGATE_BUILD_TESTS=ON -DSKYGATE_ENABLE_HIGH_PRECISION_EPHEMERIS=ON`: FAIL, configure blocked because `calceph` is not installed.
- Remaining concerns:
  - The high-precision-only `skygate-ephemeris-frame-transformer-tests` target could not be built or run in this environment because high-precision configuration requires missing `calceph`.
