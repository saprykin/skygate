## Task
- ID: HP-020
- Title: Add ephemeris data cache metadata persistence

## Status
READY

## Acceptance criteria claimed
- [x] `EphemerisDataCacheSnapshot` added separately from catalog cache snapshots
- [x] Installed kernel, EOP, leap-second, Delta T, revision, and last-update metadata persists through `SkySettingsStore`
- [x] Clear-cache behavior removes ephemeris metadata and returns to bundled fallback metadata
- [x] Settings tests cover save/load, partial metadata, malformed metadata, clear behavior, revision persistence, and catalog-cache isolation
- [x] Touched C++ files were formatted with `clang-format`

## Files changed
- `apps/skygate-ui/src/settings/SkySettingsStore.hpp`
- `apps/skygate-ui/src/settings/SkySettingsStore.cpp`
- `apps/skygate-ui/tests/settings/SkySettingsStoreTests.cpp`

## Important notes
- Focused verification passed:
  `ctest --test-dir build-ralph -R '^skygate-ui-settings-store-tests$' --output-on-failure`.
- `cmake --build build-ralph -j2` passed after configuring `build-ralph` with UI enabled.
- Full verification reported 112/113 tests passing. The only failure was the unrelated existing QML footer popup toolbar toggle regression in
  `skygate-ui-qml-main-window-tests`, recorded as follow-up task HP-054 in `IMPLEMENTATION_PLAN.md`.

## Review fixes
- Review verdict addressed: NEEDS_FIX
- Findings addressed:
  - Finding title: Installed ephemeris paths do not round-trip exactly
  - Action: Fixed
  - Notes: `loadEphemerisDataCache()` now reads installed kernel, EOP, and related installed metadata fields without trimming, preserving saved path values exactly. A settings-store regression test covers installed kernel and EOP paths with surrounding spaces.
- Files changed during fix pass:
  - `apps/skygate-ui/src/settings/SkySettingsStore.cpp`
  - `apps/skygate-ui/tests/settings/SkySettingsStoreTests.cpp`
  - `.ralph/high-precision-ephemeris-engine/HP-020/implementation.md`
  - `.ralph/high-precision-ephemeris-engine/HP-020/fix.md`
- Tests run after fix:
  - `git diff --check`: PASS
  - `cmake --build build-ralph --target skygate-ui-settings-store-tests -j2`: PASS
  - `ctest --test-dir build-ralph -R '^skygate-ui-settings-store-tests$' --output-on-failure`: PASS
  - `ctest --test-dir build-ralph --output-on-failure`: FAIL, only the unrelated known `skygate-ui-qml-main-window-tests` footer popup toolbar toggle failure remains.
- Remaining concerns:
  - Full-suite verification still has the unrelated QML main-window failure already recorded as HP-054.
