# Verdict

PASS

# Task verified

- ID: HP-014A
- Title: Implement TT and TDB conversion policy
- Source: IMPLEMENTATION_PLAN.md; spec/high-precision-ephemeris-engine.md
- Base ref: a2dabde59f13e86982fc7564eb2affcebb8c2eef
- Head ref: 0e0d6dd

# Summary

HP-014A extends `LeapSecondTimeScaleService` to convert TT to TDB, TDB back to
TT, and UTC/TAI to TDB through the existing TT conversion path. The
implementation preserves two-part Julian date handling, reports the documented
TT/TDB approximation warning, exposes the ERFA `eraDtdb` wrapper when high
precision is enabled, and adds focused time-scale service tests. The review
passed with no findings, the fix pass made no source changes, and the task is
ready for acceptance.

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

No review findings.

# Findings

No findings.

# Test assessment

Focused tests exist in
`libs/skygate-ephemeris/tests/highprecision/TimeScaleServiceTests.cpp` for
TT-to-TDB conversion, TT/TDB round-trip precision with subsecond input, and UTC
to TDB routing through TT. Existing UTC/TAI/TT, leap-second, and fallback tests
remain in the same suite.

Tests run:

- `cmake --build build-ralph --target skygate-ephemeris-time-scale-service-tests`
  - PASS
- `ctest --test-dir build-ralph -R '^skygate-ephemeris-time-scale-service-tests$' --output-on-failure`
  - PASS
- `ctest --test-dir build-ralph -E '^skygate-ui-qml-main-window-tests$' --output-on-failure`
  - PASS, 110/110 tests passed
- `ctest --test-dir build-ralph --output-on-failure`
  - 110/111 tests passed; the only failure was
    `skygate-ui-qml-main-window-tests`,
    `QmlMainWindowTests::footerPopupToolbarToggleClickClosesPopupAndTogglesToolbar()`
    at `apps/skygate-ui/tests/qml/QmlMainWindowTests.cpp(219)`, matching the
    pre-existing HP-053 issue noted by the implementation handoff.

The current `build-ralph` tree is configured with
`SKYGATE_ENABLE_HIGH_PRECISION_EPHEMERIS=OFF`, so the runnable coverage verifies
the contained fallback approximation path rather than the ERFA-linked path.

# Regression risk

Low

The source changes are concentrated in `TimeScaleService`, the ERFA wrapper
surface, and focused tests. The relevant target and all non-excluded tests pass;
the only full-suite failure is a separately tracked QML regression unrelated to
TT/TDB conversion.

# Out-of-scope observations

- The verifier prompt names `specs/high-precision-ephemeris-engine.md`, but the
  repository contains the relevant specification at
  `spec/high-precision-ephemeris-engine.md`.
- The full CTest suite still fails only in the pre-existing
  `skygate-ui-qml-main-window-tests` footer popup toolbar toggle case tracked
  by HP-053.

# Final recommendation

PASS: ready for final acceptance or merge.
