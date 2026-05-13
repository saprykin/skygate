## Task
- ID: HP-013
- Title: Add Delta T data/model provider

## Status
READY

## Acceptance criteria claimed
- [x] Delta T provider interface added
- [x] Table-backed Delta T data/model loader added
- [x] Version, source, validity range, expiration, and stale-state metadata exposed
- [x] Ancient-date fallback model metadata exposed
- [x] Tests added for present data, missing data, malformed data, ancient fallback metadata, stale data, and validity ranges
- [x] Existing tests pass

## Files changed
- `libs/skygate-ephemeris/include/skygate/ephemeris/DeltaTProvider.hpp`
- `libs/skygate-ephemeris/include/skygate/ephemeris/EphemerisDataSnapshot.hpp`
- `libs/skygate-ephemeris/src/engine/highprecision/DeltaTProvider.cpp`
- `libs/skygate-ephemeris/tests/highprecision/DeltaTProviderTests.cpp`
- `libs/skygate-ephemeris/CMakeLists.txt`
- `libs/skygate-ephemeris/tests/CMakeLists.txt`

## Important notes
- `build-ralph` is configured with `SKYGATE_ENABLE_HIGH_PRECISION_EPHEMERIS=OFF`; the Delta T provider has no CALCEPH/zstd/ERFA dependency and is built in that configuration.
- Verification run: `cmake --build build-ralph` and `ctest --test-dir build-ralph --output-on-failure`, passing 53/53 tests.
