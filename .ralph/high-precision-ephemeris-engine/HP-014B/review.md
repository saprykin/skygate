## Verdict

PASS

## Task reviewed

- ID: HP-014B
- Title: Implement UT1 conversion policy
- Source: IMPLEMENTATION_PLAN.md; spec/high-precision-ephemeris-engine.md
- Base ref: 7967dc7a5226bf4eaed116722d70f0e3c22ee3b6
- Head ref: 0f98337c2949c0beb7ddf194f17c1437f9e42de5

## Summary

HP-014B extends `LeapSecondTimeScaleService` with UT1 conversions via Earth-orientation UT1-UTC samples, reverse UT1-to-UTC conversion, and an explicitly enabled Delta T fallback path for missing or out-of-range EOP data. The implementation is scoped to the time-scale service and has focused tests for exact/interpolated EOP samples, reverse conversion, degraded EOP warnings, Delta T fallback, and disallowed fallback failure. I found no task-scoped issues requiring changes.

## Checks performed

- [x] Read active task
- [x] Read implementation handoff
- [x] Read relevant specs
- [x] Inspected git diff
- [x] Inspected relevant tests
- [x] Ran relevant tests, if safe/applicable

## Findings

No findings.

## Test assessment

Focused coverage exists in `skygate-ephemeris-time-scale-service-tests` for UTC-to-UT1 exact EOP samples, interpolation, UT1-to-UTC round-trip, predicted and stale EOP warnings, Delta T fallback for ancient out-of-range EOP, and missing-EOP failure when fallback is disabled. Existing EOP and Delta T provider tests continue to cover provider-level range and metadata behavior.

Commands run:
- `ctest --test-dir build-ralph -R skygate-ephemeris-time-scale-service-tests --output-on-failure`: passed.
- `cmake --build build-ralph -j2`: passed.
- `ctest --test-dir build-ralph --output-on-failure`: 110/111 passed; the only failure was `skygate-ui-qml-main-window-tests`, matching the handoff's pre-existing HP-053 UI failure.

## Regression risk

Low

The changes are limited to `TimeScaleService` and its tests, with existing UTC/TAI/TT/TDB paths preserved and new UT1 branches routing through existing conversion helpers. The remaining full-suite failure is unrelated QML behavior already tracked separately.

## Out-of-scope observations

- `skygate-ui-qml-main-window-tests` still fails in `QmlMainWindowTests::footerPopupToolbarToggleClickClosesPopupAndTogglesToolbar()` at `apps/skygate-ui/tests/qml/QmlMainWindowTests.cpp(219)`, consistent with the implementation handoff and HP-053.

## Final recommendation

PASS: ready for final verification.
