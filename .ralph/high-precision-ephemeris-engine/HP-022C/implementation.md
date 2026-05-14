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

## Review fixes
- Review verdict addressed: NEEDS_FIX
- Findings addressed:
  - Expected versions and validity ranges are not verified
    - Action: Fixed
    - Notes: Extended expected staged components with optional expected version and required validity range fields, reject version mismatches, and require staged asset validity coverage before payload verification.
  - Standalone metadata validation accepts incomplete validity-range labels
    - Action: Fixed
    - Notes: Standalone staged verification now rejects assets whose validity range lacks an id or display name.
- Files changed during fix pass:
  - `libs/skygate-ephemeris/include/skygate/ephemeris/EphemerisDataActivation.hpp`
  - `libs/skygate-ephemeris/src/engine/highprecision/EphemerisDataActivation.cpp`
  - `libs/skygate-ephemeris/tests/highprecision/EphemerisDataActivationTests.cpp`
  - `.ralph/high-precision-ephemeris-engine/HP-022C/implementation.md`
  - `.ralph/high-precision-ephemeris-engine/HP-022C/fix.md`
- Tests run after fix:
  - `cmake --build build-ralph --target skygate-ephemeris-data-activation-tests -j 2`: PASS
  - `ctest --test-dir build-ralph -R skygate-ephemeris-data-activation-tests --output-on-failure`: PASS
  - `ctest --test-dir build-ralph --output-on-failure`: FAIL, 119/120 passed with only the known unrelated `skygate-ui-qml-main-window-tests::footerPopupToolbarToggleClickClosesPopupAndTogglesToolbar()` failure.
- Remaining concerns:
  - The recurring unrelated QML footer popup toolbar failure remains outside HP-022C.
