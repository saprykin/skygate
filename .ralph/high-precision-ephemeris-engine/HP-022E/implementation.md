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

## Review fixes
- Review verdict addressed: NEEDS_FIX
- Findings addressed:
  - Download cancellation requirement is not implemented or tested
    - Action: Fixed
    - Notes: Added a cancellable file-backed ephemeris staging transfer API on
      `SkyEphemerisDataManager`, wired to both the manager cancellation flag and
      per-request callback. Added download cancellation coverage that preserves
      active settings/revision and retains partial staging by policy.
  - Activation can commit after cancellation is requested
    - Action: Fixed
    - Notes: Added activation cancellation checks after payload copy, after
      flush, and before commit, with `QSaveFile::cancelWriting()` on canceled
      paths. Added coverage for cancellation after payload copy and before
      commit.
  - Required pre-activation cancellation path lacks coverage
    - Action: Fixed
    - Notes: Added manager coverage for cancellation after successful staged
      verification and before the first activation write.
- Files changed during fix pass:
  - `apps/skygate-ui/src/ephemeris/SkyEphemerisDataManager.hpp`
  - `apps/skygate-ui/src/ephemeris/SkyEphemerisDataManager.cpp`
  - `apps/skygate-ui/tests/ephemeris/SkyEphemerisDataManagerTests.cpp`
  - `libs/skygate-ephemeris/src/engine/highprecision/EphemerisDataActivation.cpp`
  - `libs/skygate-ephemeris/tests/highprecision/EphemerisDataActivationTests.cpp`
  - `.ralph/high-precision-ephemeris-engine/HP-022E/implementation.md`
  - `.ralph/high-precision-ephemeris-engine/HP-022E/fix.md`
- Tests run after fix:
  - `cmake --build build-ralph --target skygate-ephemeris-data-activation-tests skygate-ui-sky-ephemeris-data-manager-tests`: PASS
  - `ctest --test-dir build-ralph --output-on-failure -R 'skygate-ephemeris-data-activation-tests|skygate-ui-sky-ephemeris-data-manager-tests'`: PASS
  - `ctest --test-dir build-ralph --output-on-failure`: PASS, 120/120 tests passed; tests 30 and 31 skipped by the current CALCEPH-dependent build configuration.
- Remaining concerns: None.
