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

## Review fixes
- Review verdict addressed: NEEDS_FIX
- Findings addressed:
  - Controller Drops Profile And Update Settings
    - Action: Fixed
    - Notes: The controller now owns the full ephemeris user settings snapshot,
      preserves non-engine fields during load/save, and has controller
      round-trip coverage for profile/update fields.
  - Ephemeris Reload Does Not Notify Scene Consumers
    - Action: Fixed
    - Notes: Loading persisted ephemeris settings now emits
      `skyContextChanged()` after rebuilding the ephemeris engine, with
      controller-level signal coverage.
- Files changed during fix pass:
  - `apps/skygate-ui/src/app/SkyContextController.hpp`
  - `apps/skygate-ui/src/app/SkyContextController.cpp`
  - `apps/skygate-ui/src/app/SkyContextControllerSettings.cpp`
  - `apps/skygate-ui/tests/app/SkyContextControllerEphemerisSettingsTests.cpp`
  - `apps/skygate-ui/tests/CMakeLists.txt`
- Tests run after fix:
  - `cmake -S . -B build-ralph`: PASS
  - `cmake --build build-ralph --target
    skygate-ui-context-controller-ephemeris-settings-tests
    skygate-ui-settings-store-tests skygate-ui-sky-settings-codecs-tests -j2`:
    PASS
  - `ctest --test-dir build-ralph --output-on-failure -R`
    `'ephemeris-settings|settings-store|sky-settings-codecs'`: PASS
  - `ctest --test-dir build-ralph --output-on-failure`: PASS, 124/124 tests
    passed, with the two CALCEPH-dependent tests skipped.
- Remaining concerns: None.
