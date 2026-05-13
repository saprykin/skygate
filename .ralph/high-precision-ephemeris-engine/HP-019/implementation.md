## Task
- ID: HP-019
- Title: Add zstd archive handling and first-use activation

## Status
READY

## Acceptance criteria claimed
- [x] Public activation API added for ephemeris data assets
- [x] Zstd compressed assets are streamed into a writable cache target
- [x] Activated asset bytes are verified with SHA-256 and expected size metadata
- [x] Corrupt archives and checksum mismatches fail without activating cache files
- [x] Existing valid cache files are treated as idempotently already active
- [x] Large solar-system kernel assets are rejected from Qt resource paths
- [x] Tests added for activation success, corrupt archive, checksum mismatch, idempotency, and Qt-resource rejection
- [x] Existing tests pass

## Files changed
- `libs/skygate-ephemeris/include/skygate/ephemeris/EphemerisDataActivation.hpp`
- `libs/skygate-ephemeris/src/engine/highprecision/EphemerisDataActivation.cpp`
- `libs/skygate-ephemeris/CMakeLists.txt`
- `libs/skygate-ephemeris/tests/CMakeLists.txt`
- `libs/skygate-ephemeris/tests/highprecision/EphemerisDataActivationTests.cpp`

## Important notes
- Verification used `build-ralph`; `ctest --test-dir build-ralph --output-on-failure` passed 56/56 tests.
