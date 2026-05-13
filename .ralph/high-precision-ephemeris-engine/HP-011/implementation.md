## Task
- ID: HP-011
- Title: Add leap-second table loading

## Status
READY

## Acceptance criteria claimed
- [x] `ILeapSecondProvider` interface added
- [x] Table-backed leap-second provider added
- [x] Leap-second table loading from an ephemeris data snapshot added
- [x] Table version and validity range metadata exposed
- [x] Missing, malformed, and stale table status reported without performing time-scale conversion policy
- [x] Tests added for valid table, malformed table, missing table, stale table, and validity-range metadata
- [x] Existing tests pass

## Files changed
- `libs/skygate-ephemeris/CMakeLists.txt`
- `libs/skygate-ephemeris/include/skygate/ephemeris/EphemerisDataSnapshot.hpp`
- `libs/skygate-ephemeris/include/skygate/ephemeris/LeapSecondProvider.hpp`
- `libs/skygate-ephemeris/src/engine/highprecision/LeapSecondProvider.cpp`
- `libs/skygate-ephemeris/tests/CMakeLists.txt`
- `libs/skygate-ephemeris/tests/highprecision/LeapSecondProviderTests.cpp`

## Important notes
- Verification used `core-debug` configured into `build-ralph`.
- `ctest --test-dir build-ralph --output-on-failure` passed with 51/51 tests.
