## Verdict

NEEDS_FIX

## Task reviewed

- ID: HP-020
- Title: Add ephemeris data cache metadata persistence
- Source: IMPLEMENTATION_PLAN.md / spec/high-precision-ephemeris-engine.md
- Base ref: 34e27d5e5b914179b870cd848f796a6422097a18
- Head ref: 4b3674bf9d450c61634cbee52d6c0d24ef8aa0da

## Summary

HP-020 adds a separate `EphemerisDataCacheSnapshot` and QSettings-backed save/load/clear methods, with focused settings-store tests for persistence, defaults, clearing, and catalog-cache isolation. The implementation is scoped to the requested settings-store surface and the focused tests pass, but installed file paths are normalized on load, so path metadata does not persist exactly.

## Checks performed

- [x] Read active task
- [x] Read implementation handoff
- [x] Read relevant specs
- [x] Inspected git diff
- [x] Inspected relevant tests
- [x] Ran relevant tests, if safe/applicable

## Findings

### Finding 1: Installed ephemeris paths do not round-trip exactly

Severity: MAJOR
File: `apps/skygate-ui/src/settings/SkySettingsStore.cpp`
Lines/functions: lines 75-77, 374-380; `readTrimmedStringSetting`, `SkySettingsStore::loadEphemerisDataCache`

Problem:
`loadEphemerisDataCache()` reads `installedKernelPath` and `installedEarthOrientationPath` through `readTrimmedStringSetting()`, which trims leading and trailing whitespace from every loaded metadata string. `saveEphemerisDataCache()` writes these paths without trimming, so a saved path containing leading or trailing whitespace loads back as a different path.

Why it matters:
HP-020 requires installed kernel and EOP path metadata to persist through `SkySettingsStore`. On platforms and filesystems where such paths are valid, trimming can make the app look for a different kernel or EOP file after restart, breaking installed ephemeris data activation even though the metadata was saved successfully.

Recommended fix:
Load path fields with `settings.value(...).toString()` so they round-trip exactly. Keep trimming/default normalization only for fields where blank values should fall back, such as `dataRevisionToken` and `lastUpdateResult`, and add a settings-store test that saves and reloads path values with surrounding spaces to lock the behavior down.

## Test assessment

Focused coverage exists in `skygate-ui-settings-store-tests` for save/load, partial metadata, blank revision/result fallback, clear behavior, revision persistence, and catalog-cache isolation. The focused target builds and the focused CTest entry passes. The tests do not currently catch exact path round-tripping for installed kernel and EOP paths.

Commands run:
- `git diff --check HEAD^..HEAD`
- `cmake --build build-ralph --target skygate-ui-settings-store-tests -j2`
- `ctest --test-dir build-ralph -R '^skygate-ui-settings-store-tests$' --output-on-failure`
- `ctest --test-dir build-ralph --output-on-failure`

The full suite reports 112/113 tests passing. The only failure is the unrelated pre-existing `skygate-ui-qml-main-window-tests` failure in `QmlMainWindowTests::footerPopupToolbarToggleClickClosesPopupAndTogglesToolbar()` at `apps/skygate-ui/tests/qml/QmlMainWindowTests.cpp(219)`.

## Regression risk

Medium

The new settings surface is small and isolated, but incorrect path persistence can prevent later ephemeris data manager work from finding installed kernel or EOP assets after restart.

## Out-of-scope observations

- The review prompt references `specs/high-precision-ephemeris-engine.md`, but this checkout stores the specification at `spec/high-precision-ephemeris-engine.md`.
- The full-suite QML main-window failure is already recorded as a separate follow-up and is unrelated to HP-020.

## Final recommendation

NEEDS_FIX: fix listed findings, then rerun review.
