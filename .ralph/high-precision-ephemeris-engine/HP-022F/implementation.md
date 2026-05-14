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
- [x] Harness covers injected activation cancellation and active-data
  preservation
- [x] Existing tests pass

## Files changed
- `apps/skygate-ui/tests/ephemeris/SkyEphemerisDataManagerTests.cpp`
- `.ralph/high-precision-ephemeris-engine/HP-022F/implementation.md`

## Important notes
- Full `ctest --test-dir build-ralph --output-on-failure` passed 120/120
  test entries, with the existing CALCEPH-dependent kernel/vector tests
  reported as skipped by the current build configuration.

## Review fixes

### Review verdict addressed
- NEEDS_FIX

### Findings addressed
- Finding title: Source payload write is hidden behind Q_ASSERT
  - Action: Fixed
  - Notes: The harness now writes the source payload unconditionally and fails
    the test with `QTest::qFail` if the write cannot be completed.
- Finding title: Harness does not cover interrupted install failure
  - Action: Fixed
  - Notes: Added a harness-based activation I/O failure case that verifies the
    staged set, fails during activation, and checks active data, settings,
    revision, failed cache cleanup, and retained staging.

### Files changed during fix pass
- `apps/skygate-ui/tests/ephemeris/SkyEphemerisDataManagerTests.cpp`
- `.ralph/high-precision-ephemeris-engine/HP-022F/implementation.md`
- `.ralph/high-precision-ephemeris-engine/HP-022F/fix.md`

### Tests run after fix
- `clang-format -i`
  `apps/skygate-ui/tests/ephemeris/SkyEphemerisDataManagerTests.cpp`
- `cmake --build build-ralph --target`
  `skygate-ui-sky-ephemeris-data-manager-tests`
- `ctest --test-dir build-ralph --output-on-failure -R`
  `'^skygate-ui-sky-ephemeris-data-manager-tests$'`
- `ctest --test-dir build-ralph --output-on-failure`

### Remaining concerns
- None.
