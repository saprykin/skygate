# Verdict

PASS

# Task verified

- ID: HP-040
- Title: Decouple engine ownership from catalog rebuilds
- Source: IMPLEMENTATION_PLAN.md / specs/high-precision-ephemeris-engine.md
- Base ref: f921b56f5d93115ac8d6889a0be9fd1180f894ed
- Head ref: 8347bfaa4bea277534bf764c725bb7a58a8499c2

# Summary

HP-040 moved ephemeris-engine ownership out of active catalog runtime and manager
objects, leaving catalog rebuilds responsible for active star/deep-sky body
lists while `SkyContextController` owns and rebuilds the engine from catalog
bodies and active ephemeris data. The review identified that high-precision
engine selection could be downgraded during controller rebuilds. The fix now
carries controller-side high-precision factory inputs into rebuild requests and
retains an already-active high-precision engine when a fallback rebuild would
otherwise silently replace it. Focused HP-040 tests pass, and the remaining
full-suite failure is the documented out-of-scope HP-057 QML toolbar failure.
The task is ready for acceptance.

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

- Finding: High-precision engine selection is downgraded during controller rebuild
  - Original severity: MAJOR
  - Closure status: Resolved
  - Notes: `SkyContextController::rebuildEphemerisEngine()` now passes the configured data manifest, data-set manifest, active data snapshot, time-scale service, Earth-orientation provider, CALCEPH runtime, and diagnostics sink to the factory. It also preserves an existing high-precision engine instead of replacing it with a simple fallback when high-precision inputs are incomplete.

# Findings

No findings.

# Test assessment

Focused HP-040 coverage exists in `skygate-ui-active-catalog-builder-tests`,
`skygate-ui-sky-catalog-runtime-tests`, and
`skygate-ui-sky-ephemeris-data-manager-tests`. The new data manager tests cover
catalog changes preserving active ephemeris data selection and selected
high-precision engine configuration, and active ephemeris data changes
preserving selected high-precision engine configuration.

Commands run:

- `git diff --check f921b56..HEAD`: PASS
- `ctest --test-dir build-ralph -R 'skygate-ui-(sky-catalog-runtime|sky-ephemeris-data-manager|active-catalog-builder)-tests' --output-on-failure`: PASS, 3/3 tests passed.
- `ctest --test-dir build-ralph --output-on-failure`: FAIL, 119/120 tests passed. The only failure was the known HP-057 out-of-scope `skygate-ui-qml-main-window-tests` failure in `QmlMainWindowTests::footerPopupToolbarToggleClickClosesPopupAndTogglesToolbar()` at `apps/skygate-ui/tests/qml/QmlMainWindowTests.cpp(219)`.

# Regression risk

Low

The ownership change is scoped to the catalog/runtime/controller boundary and
has focused coverage for active data and engine configuration preservation.
The remaining full-suite failure is already tracked separately as HP-057 and is
unrelated to engine ownership or catalog rebuild behavior.

# Out-of-scope observations

- `skygate-ui-qml-main-window-tests` still fails in
  `QmlMainWindowTests::footerPopupToolbarToggleClickClosesPopupAndTogglesToolbar()`.
  This is already recorded as HP-057.
- `skygate-ephemeris-calceph-kernel-provider-tests` and
  `skygate-ephemeris-solar-system-state-calculator-tests` were skipped by the
  current test configuration.

# Final recommendation

PASS: ready for final acceptance or merge.
