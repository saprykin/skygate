## Task
- ID: HP-039
- Title: Add ephemeris user settings snapshot

## Status
READY

## Acceptance criteria claimed
- [x] Ephemeris user settings added to the persisted state snapshot
- [x] Selected engine kind and correction flags persisted
- [x] Refraction and atmosphere defaults persisted
- [x] Preferred data profile and update settings persisted
- [x] Ephemeris data cache metadata remains separate from user settings
- [x] Malformed and partial settings fall back safely
- [x] Split/merge codec coverage added
- [x] Existing tests pass

## Files changed
- `apps/skygate-ui/src/settings/SkySettingsStore.hpp`
- `apps/skygate-ui/src/settings/SkySettingsSnapshotCodecs.hpp`
- `apps/skygate-ui/src/settings/SkySettingsSnapshotCodecs.cpp`
- `apps/skygate-ui/src/app/SkyContextControllerSettings.cpp`
- `apps/skygate-ui/tests/settings/SkySettingsCodecsTests.cpp`
- `apps/skygate-ui/tests/settings/SkySettingsStoreTests.cpp`

## Important notes
- Cache-only ephemeris data metadata can set the shared settings version key.
  The controller now applies ephemeris user settings only when the ephemeris
  settings group is present, preserving an explicitly configured engine across
  data-cache-only loads.
- Verification: `cmake --build build-ralph -j2` passed.
- Verification: `ctest --test-dir build-ralph --output-on-failure` passed
  123/123 tests, with the two configured CALCEPH-dependent tests skipped.
