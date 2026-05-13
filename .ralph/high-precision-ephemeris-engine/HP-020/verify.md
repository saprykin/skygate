# Verdict

PASS

# Task verified

- ID: HP-020
- Title: Add ephemeris data cache metadata persistence
- Source: IMPLEMENTATION_PLAN.md / spec/high-precision-ephemeris-engine.md
- Base ref: 34e27d5e5b914179b870cd848f796a6422097a18
- Head ref: 84e97aee328f36e24c9f7200d3d0d2aa143f230f

# Summary

HP-020 adds a separate `EphemerisDataCacheSnapshot` to `SkySettingsStore`, persists installed ephemeris kernel/EOP/leap-second/Delta T metadata plus revision and update-result fields, and adds clear-cache behavior that returns ephemeris metadata to bundled fallback defaults without clearing catalog cache data. The review finding about exact installed path round-tripping was fixed by loading path fields without trimming, and the new regression test covers that behavior. Focused settings-store verification passes. The full suite still has the unrelated QML main-window footer popup failure already tracked as HP-054, so HP-020 is ready for acceptance.

# Checks performed

- [x] Read active task
- [x] Read implementation handoff
- [x] Read review report
- [x] Read fixer report, if present
- [x] Read relevant specs
- [x] Inspected git history
- [x] Inspected git diff
- [x] Inspected relevant tests
- [x] Ran relevant tests, if safe/applicable

# Review/fix closure

- Finding: Installed ephemeris paths do not round-trip exactly
  - Original severity: MAJOR
  - Closure status: Resolved
  - Notes: `SkySettingsStore::loadEphemerisDataCache()` now reads installed kernel and EOP path fields directly from `QSettings` without trimming, and `ephemerisDataCachePathsRoundTripExactly()` verifies paths with surrounding spaces round-trip unchanged.

# Findings

No findings.

# Test assessment

Relevant coverage exists in `skygate-ui-settings-store-tests` for ephemeris data cache save/load, partial metadata, malformed blank revision/result fallback, clear-cache behavior, data revision persistence, catalog-cache isolation, and exact installed path round-tripping after the fix.

Commands run:
- `git diff --check 34e27d5..HEAD`: PASS
- `cmake --build build-ralph --target skygate-ui-settings-store-tests -j2`: PASS
- `ctest --test-dir build-ralph -R '^skygate-ui-settings-store-tests$' --output-on-failure`: PASS
- `ctest --test-dir build-ralph --output-on-failure`: FAIL, with 112/113 tests passing and only the unrelated `skygate-ui-qml-main-window-tests` failure in `QmlMainWindowTests::footerPopupToolbarToggleClickClosesPopupAndTogglesToolbar()` at `apps/skygate-ui/tests/qml/QmlMainWindowTests.cpp(219)`.

# Regression risk

Low

The implemented behavior is confined to settings metadata persistence and focused tests cover the new API surface and the review fix. The full-suite failure is outside HP-020 and is already captured as follow-up HP-054.

# Out-of-scope observations

- The verifier prompt references `specs/high-precision-ephemeris-engine.md`, but this checkout stores the specification at `spec/high-precision-ephemeris-engine.md`.
- The full-suite QML footer popup toolbar toggle failure remains present and is tracked separately as HP-054.

# Final recommendation

PASS: ready for final acceptance or merge.
