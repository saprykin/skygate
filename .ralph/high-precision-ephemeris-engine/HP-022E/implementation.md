## Task
- ID: HP-022E
- Title: Add cancellation and failed-update preservation semantics

## Status
READY

## Acceptance criteria claimed
- [x] Staged update verification can be canceled before or during validation
- [x] Asset activation can be canceled before or during cache writes
- [x] Manager-level cancellation preserves the active ephemeris snapshot and revision
- [x] Failed activation and failed metadata persistence clean inactive activation cache output
- [x] Staging can be retained for restart/resume, or cleaned on cancellation by policy
- [x] Tests added for cancellation, failure cleanup, retained staging, and active-data preservation
- [x] Full `build-ralph` test suite passes

## Files changed
- `libs/skygate-ephemeris/include/skygate/ephemeris/EphemerisDataActivation.hpp`
- `libs/skygate-ephemeris/src/engine/highprecision/EphemerisDataActivation.cpp`
- `libs/skygate-ephemeris/tests/highprecision/EphemerisDataActivationTests.cpp`
- `apps/skygate-ui/src/ephemeris/SkyEphemerisDataManager.hpp`
- `apps/skygate-ui/src/ephemeris/SkyEphemerisDataManager.cpp`
- `apps/skygate-ui/tests/ephemeris/SkyEphemerisDataManagerTests.cpp`

## Important notes
- Verification run: `cmake --build build-ralph`
- Verification run: `ctest --test-dir build-ralph --output-on-failure`
- Result: 120/120 tests passed; existing CALCEPH-dependent tests 30 and 31 were skipped by the current build configuration.
