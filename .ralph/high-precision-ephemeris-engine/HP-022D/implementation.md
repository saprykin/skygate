## Task
- ID: HP-022D
- Title: Atomically activate verified ephemeris data

## Status
READY

## Acceptance criteria claimed
- [x] Complete verified staged update sets activate into a revision-scoped cache location.
- [x] `EphemerisDataCacheSnapshot` is persisted only after all selected assets activate successfully.
- [x] Ephemeris data revision changes and active-data signals emit only after successful activation.
- [x] Activation failures preserve the active in-memory snapshot and persisted settings.
- [x] Metadata persistence failure preserves the active in-memory snapshot.
- [x] Focused ephemeris data manager tests pass.

## Files changed
- `apps/skygate-ui/src/ephemeris/SkyEphemerisDataManager.hpp`
- `apps/skygate-ui/src/ephemeris/SkyEphemerisDataManager.cpp`
- `apps/skygate-ui/tests/ephemeris/SkyEphemerisDataManagerTests.cpp`

## Important notes
- Full `ctest --test-dir build-ralph --output-on-failure` was run after a full build. It passed 119/120 tests; the only failure was the pre-existing `skygate-ui-qml-main-window-tests` footer popup toolbar toggle failure already tracked by HP-058.
- High-precision CALCEPH kernel tests were skipped by the existing test configuration.
