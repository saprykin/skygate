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

## Review fixes
- Review verdict addressed: NEEDS_FIX
- Findings addressed:
  - Finding title: Estimated EOP degradation is not modeled or tested
  - Action: Fixed
  - Notes: Added explicit estimated Earth-orientation data status and sample warning support, row/sample propagation, time-scale warning propagation, provider coverage, and frame-transform degradation coverage for estimated EOP metadata.
  - Finding title: Skipped-stage metadata coverage is missing
  - Action: Fixed
  - Notes: Same-rank frame-boundary identity transforms now record a valid non-applied stage when the requested source and target frames differ, and the ICRS-to-GCRS identity test asserts the skipped-stage metadata.
- Files changed during fix pass:
  - `libs/skygate-ephemeris/include/skygate/ephemeris/EarthOrientationProvider.hpp`
  - `libs/skygate-ephemeris/include/skygate/ephemeris/TimeScaleService.hpp`
  - `libs/skygate-ephemeris/src/engine/highprecision/EarthOrientationProvider.cpp`
  - `libs/skygate-ephemeris/src/engine/highprecision/FrameTransformer.cpp`
  - `libs/skygate-ephemeris/src/engine/highprecision/TimeScaleService.cpp`
  - `libs/skygate-ephemeris/tests/highprecision/EarthOrientationProviderTests.cpp`
  - `libs/skygate-ephemeris/tests/highprecision/FrameTransformerTests.cpp`
  - `.ralph/high-precision-ephemeris-engine/HP-026/implementation.md`
  - `.ralph/high-precision-ephemeris-engine/HP-026/fix.md`
- Tests run after fix:
  - `clang-format -i libs/skygate-ephemeris/include/skygate/ephemeris/EarthOrientationProvider.hpp libs/skygate-ephemeris/include/skygate/ephemeris/TimeScaleService.hpp libs/skygate-ephemeris/src/engine/highprecision/EarthOrientationProvider.cpp libs/skygate-ephemeris/src/engine/highprecision/TimeScaleService.cpp libs/skygate-ephemeris/src/engine/highprecision/FrameTransformer.cpp libs/skygate-ephemeris/tests/highprecision/EarthOrientationProviderTests.cpp libs/skygate-ephemeris/tests/highprecision/FrameTransformerTests.cpp`: PASS
  - `cmake --build build-ralph --parallel`: PASS
  - `ctest --test-dir build-ralph --output-on-failure`: PASS, 58/58 tests
  - `cmake -S . -B build-ralph -DSKYGATE_BUILD_UI=OFF -DSKYGATE_BUILD_TESTS=ON -DSKYGATE_ENABLE_HIGH_PRECISION_EPHEMERIS=ON`: FAIL, `calceph` package config not found in this environment.
  - `cmake -S . -B build-ralph -DSKYGATE_BUILD_UI=OFF -DSKYGATE_BUILD_TESTS=ON -DSKYGATE_ENABLE_HIGH_PRECISION_EPHEMERIS=OFF`: PASS
  - `cmake --build build-ralph --parallel && ctest --test-dir build-ralph --output-on-failure`: PASS, 58/58 tests
- Remaining concerns: The task-specific `skygate-ephemeris-frame-transformer-tests` target could not be generated or run locally because high-precision configuration still requires missing `calceph`.
