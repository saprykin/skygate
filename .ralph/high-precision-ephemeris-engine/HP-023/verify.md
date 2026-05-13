# Verdict

PASS

# Task verified

- ID: HP-023
- Title: Add CALCEPH kernel loading and selection provider
- Source: IMPLEMENTATION_PLAN.md / specs/high-precision-ephemeris-engine.md
- Base ref: 5c6eee3
- Head ref: 6c7cde4

# Summary

HP-023 adds a high-precision `CalcephKernelProvider`, active data snapshot kernel metadata, CMake/test wiring, and focused provider tests for profile selection, file validation, checksum/open failure, and validity range status. The review found that active kernel paths could be misidentified as a different selected manifest asset; the fix persists installed kernel asset/profile identity, exposes only matching active snapshot assets, and rejects mismatched snapshot data before opening the kernel. The task is implemented, the review finding is resolved, and the relevant tests pass. The full suite still has one unrelated QML main-window failure tracked outside HP-023.

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

- Finding: Active kernel snapshot can misidentify the selected manifest asset
  - Original severity: MAJOR
  - Closure status: Resolved
  - Notes: `SkySettingsStore` now persists installed kernel asset/profile IDs, `SkyActiveEphemerisDataSnapshot::solarSystemKernelAsset()` returns `std::nullopt` unless the requested asset ID matches the installed kernel, and `CalcephKernelProvider` rejects mismatched snapshot asset/profile IDs before file validation, checksum verification, or CALCEPH open.

# Findings

No findings.

# Test assessment

Relevant HP-023 tests exist in `skygate-ephemeris-calceph-kernel-provider-tests`, `skygate-ui-sky-ephemeris-data-manager-tests`, and `skygate-ui-settings-store-tests`. They cover modern kernel selection, optional long-range selection, missing selected assets, snapshot asset/profile mismatches, checksum mismatch, open failure, open/close ownership, date-range status, active snapshot identity filtering, and settings persistence for installed kernel identity.

Commands run:
- `cmake --build build-ralph`: PASS
- `ctest --test-dir build-ralph --output-on-failure -R 'skygate-ephemeris-calceph-kernel-provider-tests|skygate-ui-sky-ephemeris-data-manager-tests|skygate-ui-settings-store-tests'`: PASS, 3/3 tests passed
- `ctest --test-dir build-ralph --output-on-failure`: FAIL, 114/115 tests passed; the only failure was the pre-existing unrelated `skygate-ui-qml-main-window-tests` failure in `QmlMainWindowTests::footerPopupToolbarToggleClickClosesPopupAndTogglesToolbar()` at `apps/skygate-ui/tests/qml/QmlMainWindowTests.cpp(219)`

# Regression risk

Low

The new provider is isolated under the high-precision engine boundary. The settings/data-manager changes are limited to carrying installed kernel identity through the active ephemeris data snapshot, and tests now cover both the intended path and the review-identified mismatch cases.

# Out-of-scope observations

- The full suite still has the recurring unrelated `skygate-ui-qml-main-window-tests` footer popup toolbar failure. It is not caused by HP-023 and is tracked separately in `IMPLEMENTATION_PLAN.md`.
- Provider tests use a fake CALCEPH runtime for open failure coverage. That is appropriate for unit scope, but a future integration fixture with a real small kernel would strengthen end-to-end CALCEPH validation.
- Tests validate the returned validity range behavior through `statusForEpoch()` but do not directly compare every `kernelInfo()->validityRange` field.

# Final recommendation

PASS: ready for final acceptance or merge.
