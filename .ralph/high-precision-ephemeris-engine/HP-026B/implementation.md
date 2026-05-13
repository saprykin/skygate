## Task
- ID: HP-026B
- Title: Implement Earth rotation and terrestrial transforms

## Status
READY

## Acceptance criteria claimed
- [x] Added CIRS, TIRS, and ITRS frame stages through the high-precision `FrameTransformer`
- [x] Added ERFA wrappers for Earth rotation angle, TIO locator, and polar-motion matrix calculation
- [x] Used UT1 conversion and Earth-orientation samples for terrestrial transforms
- [x] Degraded transform metadata is emitted for predicted or missing Earth-orientation data
- [x] Added frame-transform tests with SOFA/ERFA reference values for CIRS/TIRS/ITRS stages
- [x] Existing tests pass in the available `build-ralph` configuration

## Files changed
- `libs/skygate-ephemeris/src/engine/highprecision/ErfaAstrometry.hpp`
- `libs/skygate-ephemeris/src/engine/highprecision/ErfaAstrometry.cpp`
- `libs/skygate-ephemeris/src/engine/highprecision/FrameTransformer.hpp`
- `libs/skygate-ephemeris/src/engine/highprecision/FrameTransformer.cpp`
- `libs/skygate-ephemeris/tests/highprecision/FrameTransformerTests.cpp`

## Important notes
- `cmake -S . -B build-ralph -DSKYGATE_BUILD_UI=OFF -DSKYGATE_BUILD_TESTS=ON -DSKYGATE_ENABLE_HIGH_PRECISION_EPHEMERIS=ON -DVCPKG_MANIFEST_FEATURES=high-precision-ephemeris` could not configure in this environment because `calceph` was not installed and `VCPKG_ROOT` was unset.
- Verified the simple-only build after restoring `SKYGATE_ENABLE_HIGH_PRECISION_EPHEMERIS=OFF`: `cmake --build build-ralph -j2`.
- Verified available tests: `ctest --test-dir build-ralph --output-on-failure` passed 58/58 tests.
