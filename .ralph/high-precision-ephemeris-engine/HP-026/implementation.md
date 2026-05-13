## Task
- ID: HP-026
- Title: Implement IAU/IERS frame transformation pipeline

## Status
READY

## Acceptance criteria claimed
- [x] HP-026A celestial frame transforms are implemented
- [x] HP-026B Earth rotation and terrestrial transforms are implemented
- [x] HP-026C FrameTransformer orchestration and per-stage metadata are implemented
- [x] Frame-transform code and tests are present under the high-precision boundary
- [x] Existing configured tests pass

## Files changed
- `.ralph/high-precision-ephemeris-engine/HP-026/implementation.md`

## Important notes
- HP-026 is an umbrella task. Its child implementation records are present for HP-026A, HP-026B, and HP-026C.
- The codebase contains `ErfaFrameTransformer` and `FrameTransformerTests` coverage for ICRS/GCRS/CIRS/TIRS/ITRS transforms, composed-stage metadata, degraded EOP/time data, and request-scoped reuse.
- `cmake --build build-ralph --parallel` passed with the current `build-ralph` configuration.
- `ctest --test-dir build-ralph --output-on-failure` passed: 58/58 tests.
- `cmake -S . -B build-ralph -DSKYGATE_BUILD_UI=OFF -DSKYGATE_BUILD_TESTS=ON -DSKYGATE_ENABLE_HIGH_PRECISION_EPHEMERIS=ON` is still blocked in this environment because `calceph` is not installed, so the gated `skygate-ephemeris-frame-transformer-tests` target could not be generated or run here.
- `build-ralph` was restored to `SKYGATE_ENABLE_HIGH_PRECISION_EPHEMERIS=OFF` after the dependency check.
