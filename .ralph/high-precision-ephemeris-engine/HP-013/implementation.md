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

## Review fixes
- Review verdict addressed: NEEDS_FIX
- Findings addressed:
  - Finding title: Validity range can exceed usable table coverage
    - Action: Fixed
    - Notes: Delta T validity range now ends at the last usable table row; `expiresAt` remains separate freshness metadata. Added coverage for a request between the last table row and expiration returning unavailable.
  - Finding title: Ancient fallback can be marked usable without a Delta T value
    - Action: Fixed
    - Notes: Ancient fallback metadata is now malformed unless it includes `ancient_fallback_delta_t_seconds`.
  - Finding title: Partial fallback ranges can fabricate missing endpoints
    - Action: Fixed
    - Notes: The loader tracks explicitly provided fallback start/end metadata and rejects partial fallback ranges.
- Files changed during fix pass:
  - `libs/skygate-ephemeris/src/engine/highprecision/DeltaTProvider.cpp`
  - `libs/skygate-ephemeris/tests/highprecision/DeltaTProviderTests.cpp`
  - `.ralph/high-precision-ephemeris-engine/HP-013/implementation.md`
  - `.ralph/high-precision-ephemeris-engine/HP-013/fix.md`
- Tests run after fix:
  - `cmake --build build-ralph --target skygate-ephemeris-delta-t-provider-tests` - PASS
  - `ctest --test-dir build-ralph -R skygate-ephemeris-delta-t-provider-tests --output-on-failure` - PASS
  - `ctest --test-dir build-ralph --output-on-failure` - PASS, 53/53 tests
- Remaining concerns: None.
