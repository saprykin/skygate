## Task
- ID: HP-022C
- Title: Verify staged update sets before activation

## Status
READY

## Acceptance criteria claimed
- [x] Manifest-selected staged assets are validated before activation
- [x] Checksums, component kinds, versions, validity ranges, and compression metadata are verified
- [x] Incomplete, mismatched, malformed, corrupt, unsupported, and successful staged sets are covered by tests
- [x] Touched C++ files were formatted with clang-format
- [x] Relevant build target passes

## Files changed
- `libs/skygate-ephemeris/include/skygate/ephemeris/EphemerisDataActivation.hpp`
- `libs/skygate-ephemeris/src/engine/highprecision/EphemerisDataActivation.cpp`
- `libs/skygate-ephemeris/tests/highprecision/EphemerisDataActivationTests.cpp`

## Important notes
- Added `verifyEphemerisStagedUpdateSet()` as the explicit pre-activation verification surface for staged ephemeris data.
- `ctest --test-dir build-ralph -R skygate-ephemeris-data-activation-tests --output-on-failure` passes.
- Full `ctest --test-dir build-ralph --output-on-failure` passed 119/120 tests; the only failure was the recurring unrelated `skygate-ui-qml-main-window-tests` footer popup toolbar toggle case.
