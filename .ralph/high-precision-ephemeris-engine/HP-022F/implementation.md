## Task
- ID: HP-022F
- Title: Add update-flow test harness and fault injection

## Status
READY

## Acceptance criteria claimed
- [x] Reusable ephemeris update-flow test harness added
- [x] Harness covers successful activation and revision signal behavior
- [x] Harness covers restart after partial download without network access
- [x] Harness covers injected checksum verification failure
- [x] Harness covers injected activation cancellation and active-data preservation
- [x] Existing tests pass

## Files changed
- `apps/skygate-ui/tests/ephemeris/SkyEphemerisDataManagerTests.cpp`
- `.ralph/high-precision-ephemeris-engine/HP-022F/implementation.md`

## Important notes
- Full `ctest --test-dir build-ralph --output-on-failure` passed 120/120
  test entries, with the existing CALCEPH-dependent kernel/vector tests
  reported as skipped by the current build configuration.
