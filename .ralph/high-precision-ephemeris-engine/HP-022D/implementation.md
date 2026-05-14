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

## Review fixes

- Review verdict addressed: NEEDS_FIX
- Findings addressed:
  - Activation can overwrite active files before the update set commits
    - Action: Fixed
    - Notes: Same-revision activations now detect whether the default revision cache root contains active snapshot paths and write to a non-active activation root instead. A regression test covers a same-token activation where one asset is written and a later asset fails; the old active files, in-memory snapshot, and persisted settings remain unchanged.
  - Activated leap-second and Delta T assets are not exposed as active data
    - Action: Fixed
    - Notes: `EphemerisDataCacheSnapshot` now persists active leap-second and Delta T paths, activation populates them, settings round-trip them, and `SkyActiveEphemerisDataSnapshot` loads both text assets from the active installed cache.
- Files changed during fix pass:
  - `apps/skygate-ui/src/ephemeris/SkyEphemerisDataManager.cpp`
  - `apps/skygate-ui/src/settings/SkySettingsStore.hpp`
  - `apps/skygate-ui/src/settings/SkySettingsStore.cpp`
  - `apps/skygate-ui/tests/ephemeris/SkyEphemerisDataManagerTests.cpp`
  - `apps/skygate-ui/tests/settings/SkySettingsStoreTests.cpp`
  - `.ralph/high-precision-ephemeris-engine/HP-022D/implementation.md`
  - `.ralph/high-precision-ephemeris-engine/HP-022D/fix.md`
- Tests run after fix:
  - `clang-format -i apps/skygate-ui/src/settings/SkySettingsStore.hpp apps/skygate-ui/src/settings/SkySettingsStore.cpp apps/skygate-ui/src/ephemeris/SkyEphemerisDataManager.cpp apps/skygate-ui/tests/settings/SkySettingsStoreTests.cpp apps/skygate-ui/tests/ephemeris/SkyEphemerisDataManagerTests.cpp`: PASS
  - `cmake --build build-ralph --target skygate-ui-sky-ephemeris-data-manager-tests`: PASS after rerunning sequentially
  - `cmake --build build-ralph --target skygate-ui-settings-store-tests`: PASS after rerunning sequentially
  - `ctest --test-dir build-ralph -R '^skygate-ui-sky-ephemeris-data-manager-tests$' --output-on-failure`: PASS
  - `ctest --test-dir build-ralph -R '^skygate-ui-settings-store-tests$' --output-on-failure`: PASS
  - `cmake --build build-ralph --target skygate-ephemeris-data-activation-tests && ctest --test-dir build-ralph -R '^skygate-ephemeris-data-activation-tests$' --output-on-failure`: PASS
  - `git diff --check`: PASS
- Remaining concerns:
  - Full-suite QML main-window footer popup failure remains out of scope and tracked by HP-058.
