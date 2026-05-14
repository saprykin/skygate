## Verdict

NEEDS_FIX

## Task reviewed

- ID: HP-040
- Title: Decouple engine ownership from catalog rebuilds
- Source: IMPLEMENTATION_PLAN.md / specs/high-precision-ephemeris-engine.md
- Base ref: f921b56f5d93115ac8d6889a0be9fd1180f894ed
- Head ref: 29b0c2afc1ad155b7ffa2c522fac5efaae580e7e

## Summary

The implementation removes ephemeris-engine ownership from active catalog runtime objects and moves the current engine pointer into `SkyContextController`. Catalog rebuilds now rebuild the controller-owned engine, and focused catalog/runtime/data-manager tests pass. However, the new rebuild path does not carry the high-precision data wiring required by the factory, so a selected or injected high-precision engine is replaced by a simple fallback on construction or catalog/data changes. The task is not complete until the controller can preserve selected engine settings and rebuild the requested engine with the required data dependencies.

## Checks performed

- [x] Read active task
- [x] Read implementation handoff
- [x] Read relevant specs
- [x] Inspected git diff
- [x] Inspected relevant tests
- [x] Ran relevant tests, if safe/applicable

## Findings

### Finding 1: High-precision engine selection is downgraded during controller rebuild

Severity: MAJOR
File: `apps/skygate-ui/src/app/SkyContextController.cpp`
Lines/functions: 59-63, 458-470, `SkyContextController::rebuildEphemerisEngine`

Problem:
`SkyContextController` captures the kind/options from an injected current engine, then immediately calls `rebuildEphemerisEngine()`. The rebuild request includes only `engineKind`, catalog bodies, options, and `activeDataSnapshot`; it does not provide the `EphemerisDataManifest`, time-scale service, Earth-orientation provider, or other high-precision dependencies required by the factory. The factory treats those missing fields as high-precision creation errors and, because the controller always uses `AllowSimpleEngineFallback`, returns a simple fallback engine. This means an existing high-precision engine or future high-precision selection is silently replaced with a simple engine during construction and again on catalog or active data changes.

Why it matters:
HP-040 explicitly requires selected engine kind/options/data wiring to move out of catalog rebuild ownership, and requires engine replacement when active ephemeris data changes. With the current wiring, the controller owns an engine but cannot preserve or rebuild the selected high-precision engine; catalog/data changes can silently degrade the app to simple mode.

Recommended fix:
Move the full high-precision factory inputs into the controller-side ownership path before rebuilding: data manifest/data-set info, time-scale service, Earth-orientation provider, kernel runtime where applicable, and diagnostics handling. If those dependencies are not yet app-owned, keep the existing engine when a high-precision rebuild cannot be constructed or expose a structured fallback decision rather than silently overwriting the selected engine. Add a controller-level test that starts with or selects a high-precision engine configuration, triggers a catalog rebuild and active ephemeris data change, and verifies the requested engine kind/options and active data wiring are preserved.

## Test assessment

Focused tests were added for catalog/runtime separation and active ephemeris data preservation. They pass:

- `ctest --test-dir build-ralph -R 'skygate-ui-(sky-catalog-runtime|sky-ephemeris-data-manager|active-catalog-builder)-tests' --output-on-failure`

The full suite was also run:

- `ctest --test-dir build-ralph --output-on-failure`

Result: 119/120 tests passed. The only failure was the known out-of-scope `skygate-ui-qml-main-window-tests`, failing `QmlMainWindowTests::footerPopupToolbarToggleClickClosesPopupAndTogglesToolbar()` at `apps/skygate-ui/tests/qml/QmlMainWindowTests.cpp(219)`.

Missing coverage: there is no test proving catalog changes preserve the selected engine kind/options, and no test proving engine changes avoid star/deep-sky re-download or reparse. The new data-manager test verifies active data snapshot preservation, but only checks that `ephemerisEngine()` remains non-null, so it does not catch the high-precision-to-simple fallback described above.

## Regression risk

Medium

The catalog-runtime decoupling itself is narrow and existing focused tests pass, but the controller now recreates the engine in more places and the missing high-precision wiring affects the central app integration path for the feature this task is preparing.

## Out-of-scope observations

- The persistent `skygate-ui-qml-main-window-tests` failure appears unrelated to HP-040 and matches the implementation handoff.
- Some touched files have broad formatting churn around line wrapping, but it does not change behavior and is not blocking.

## Final recommendation

NEEDS_FIX: fix listed findings, then rerun review.
