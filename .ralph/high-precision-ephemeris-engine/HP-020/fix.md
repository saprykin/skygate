## Task fixed
  - ID: HP-020
  - Title: Add ephemeris data cache metadata persistence
  - Source: IMPLEMENTATION_PLAN.md / spec/high-precision-ephemeris-engine.md

## Review input
  - Review verdict: NEEDS_FIX
  - Review report: .ralph/high-precision-ephemeris-engine/HP-020/review.md
  - Implementation handoff: .ralph/high-precision-ephemeris-engine/HP-020/implementation.md

## Summary
  Fixed ephemeris data cache metadata loading so installed file path fields round-trip exactly instead of being trimmed on load.

## Findings addressed
  - Finding title: Installed ephemeris paths do not round-trip exactly
  - Severity: MAJOR
  - Action: Fixed
  - File(s): apps/skygate-ui/src/settings/SkySettingsStore.cpp; apps/skygate-ui/tests/settings/SkySettingsStoreTests.cpp
  - What changed: Installed ephemeris metadata fields now load directly from QSettings without trimming. Added a regression test for installed kernel and EOP paths with surrounding spaces.
  - Why this resolves the finding: Saved path strings are returned exactly as stored, so valid filesystem paths containing leading or trailing whitespace are not changed across restart.

## Tests run
  - `git diff --check`: PASS
  - `cmake --build build-ralph --target skygate-ui-settings-store-tests -j2`: PASS
  - `ctest --test-dir build-ralph -R '^skygate-ui-settings-store-tests$' --output-on-failure`: PASS
  - `ctest --test-dir build-ralph --output-on-failure`: FAIL, unrelated known `skygate-ui-qml-main-window-tests` failure in `QmlMainWindowTests::footerPopupToolbarToggleClickClosesPopupAndTogglesToolbar()`

## Files changed
  - apps/skygate-ui/src/settings/SkySettingsStore.cpp
  - apps/skygate-ui/tests/settings/SkySettingsStoreTests.cpp
  - .ralph/high-precision-ephemeris-engine/HP-020/implementation.md
  - .ralph/high-precision-ephemeris-engine/HP-020/fix.md

## Remaining concerns
  Full-suite verification still fails only on the unrelated QML main-window footer popup toolbar toggle test already tracked outside HP-020.

## Final fixer status
  READY_FOR_REVIEW
