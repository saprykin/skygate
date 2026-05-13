# Verdict

PASS

# Task verified

- ID: HP-009
- Title: Add the high-precision engine facade boundary
- Source: IMPLEMENTATION_PLAN.md / spec/high-precision-ephemeris-engine.md
- Base ref: 0733e9f16fc86202c474fd1cd03879226502b567
- Head ref: bd7223488f041ef87e0b2b4b3c42b28ba50f076f

# Summary

HP-009 added the internal `HighPrecisionEphemerisEngine` facade under
`src/engine/highprecision/`, with focused calculator/provider boundaries,
request validation, solar-system and catalog-star dispatch, default result
assembly, and structured failed/unsupported result paths. Review found weak
option-forwarding coverage and missing `highprecision` test labeling; the fix
pass strengthened the per-request option test and labeled the new test target.
The task-specific build and tests pass, and the remaining full-suite failure is
an unrelated QML issue already tracked separately as HP-053. Final judgment:
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

- Finding: Option-forwarding test does not distinguish request options from engine defaults
  - Original severity: MAJOR
  - Closure status: Resolved
  - Notes: The test now constructs engine defaults with `AtmosphericRefraction`
    while the per-request flags are `LightTime | EarthOrientation`, then asserts
    the calculator, apparent-place calculator, result builder, and returned
    metadata all observe the request-specific flags.

- Finding: High-precision facade tests are not discoverable through the highprecision label
  - Original severity: MINOR
  - Closure status: Resolved
  - Notes: The new `skygate-ephemeris-highprecision-engine-tests` target is
    registered with `LABELS "unit;highprecision"`, and
    `ctest -L highprecision -N` discovers it.

# Findings

No findings.

# Test assessment

The HP-009 facade tests cover metadata/capabilities, solar-system dispatch,
catalog-star dispatch, request option forwarding, invalid request validation,
unsupported body status, result builder assembly, and compatibility request
behavior. I ran:

- `cmake --build build-ralph --target skygate-ephemeris-highprecision-engine-tests`: PASS
- `git diff --check 0733e9f16fc86202c474fd1cd03879226502b567..HEAD`: PASS
- `ctest --test-dir build-ralph -R skygate-ephemeris-highprecision-engine-tests --output-on-failure`: PASS
- `ctest --test-dir build-ralph -L highprecision -N`: PASS, discovers 1 test
- `ctest --test-dir build-ralph -L highprecision --output-on-failure`: PASS
- `ctest --test-dir build-ralph --output-on-failure`: FAIL, 106/107 passed

The full-suite failure is `skygate-ui-qml-main-window-tests` in
`QmlMainWindowTests::footerPopupToolbarToggleClickClosesPopupAndTogglesToolbar()`
at `apps/skygate-ui/tests/qml/QmlMainWindowTests.cpp(219)`, where
`!controller->timelineToolbarCollapsed()` returned false. This is the recurring
UI failure tracked separately by HP-053 and is unrelated to the HP-009
high-precision facade boundary.

# Regression risk

Low

The production changes are internal to the ephemeris library and the new
high-precision engine is not factory-wired for real construction yet. The
focused tests exercise the new facade boundary and the review fixes. The known
residual risk is limited to the unrelated UI test failure tracked as HP-053.

# Out-of-scope observations

- The verifier prompt refers to `specs/high-precision-ephemeris-engine.md`, but
  this workspace contains `spec/high-precision-ephemeris-engine.md`.
- `HighPrecisionEphemerisEngine` defines internal time/EOP provider interfaces
  under `skygate::ephemeris::highprecision`, while the factory request already
  forward-declares similarly named ephemeris interfaces. HP-008F may need shared
  public interfaces or adapters when real factory wiring is implemented.
- The full CTest suite still has the unrelated QML main-window failure recorded
  under HP-053.

# Final recommendation

PASS: ready for final acceptance or merge.
