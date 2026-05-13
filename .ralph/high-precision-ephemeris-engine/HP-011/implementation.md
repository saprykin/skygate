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

## Review fixes

- Review verdict addressed: NEEDS_FIX
- Findings addressed:
  - Finding title: Invalid expiration metadata is accepted as a usable table
  - Action: Fixed
  - Notes: Recognized `#@ expires` metadata now fails loading as
    `LeapSecondTableStatus::Malformed` when its date cannot be parsed or
    converted to a UTC epoch. The loader no longer returns a provider, does not
    set validity metadata, and reports a diagnostic for malformed expiration
    metadata.
- Files changed during fix pass:
  - `libs/skygate-ephemeris/src/engine/highprecision/LeapSecondProvider.cpp`
  - `libs/skygate-ephemeris/tests/highprecision/LeapSecondProviderTests.cpp`
  - `.ralph/high-precision-ephemeris-engine/HP-011/implementation.md`
  - `.ralph/high-precision-ephemeris-engine/HP-011/fix.md`
- Tests run after fix:
  - `cmake --build build-ralph --target skygate-ephemeris-leap-second-provider-tests`
    PASS
  - `ctest --test-dir build-ralph -R '^skygate-ephemeris-leap-second-provider-tests$' --output-on-failure`
    PASS
  - `ctest --test-dir build-ralph -R 'skygate-ephemeris-(api-model|leap-second-provider)-tests' --output-on-failure`
    PASS
  - `ctest --test-dir build-ralph --output-on-failure` PASS
- Remaining concerns: None.
