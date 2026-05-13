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
