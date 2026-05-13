# Verdict

PASS

# Task verified

- ID: HP-006B
- Title: Add request-based snapshot compute API
- Source: IMPLEMENTATION_PLAN.md / spec/high-precision-ephemeris-engine.md
- Base ref: 2962442dedf2cbbdbff65a0282ba1e63c612196a
- Head ref: e9c8237c65207e9170bb9d622284338feb51ee72

# Summary

HP-006B adds `IEphemerisEngine::compute(const EphemerisRequest&)`, overrides it in the simple engine, maps UTC request epochs into the existing `SkyContext` path, preserves the current `SkySnapshot` shape, and adds baseline tests for request/context equivalence and unsupported simple-engine options. The review finding about ignored request options was fixed by marking valid simple-engine results degraded with `CorrectionUnavailable` when unsupported correction or refraction options are requested. The task is ready for acceptance; the only full-suite test failure is the pre-existing HP-051 QML footer popup regression.

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

- Finding: Request options are ignored by the simple-engine request path
  - Original severity: MAJOR
  - Closure status: Resolved
  - Notes: `SimpleEphemerisEngine::compute(const EphemerisRequest&)` now explicitly checks request correction/refraction options and annotates unsupported requests as degraded with `CorrectionUnavailable` while preserving simple-engine coordinates.

# Findings

No findings.

# Test assessment

Relevant coverage exists in `skygate-ephemeris-engine-baseline-tests`. The new tests verify that a UTC `EphemerisRequest` epoch overrides the request context time and produces the same snapshot coordinates as the equivalent `SkyContext` path, and that unsupported correction/refraction options are reported through result metadata without changing simple-engine coordinates.

Tests run:

- `cmake --build build-ralph --target skygate-ephemeris-engine-baseline-tests`: PASS
- `ctest --test-dir build-ralph -R '^skygate-ephemeris-engine-baseline-tests$' --output-on-failure`: PASS
- `git diff --check 2962442dedf2cbbdbff65a0282ba1e63c612196a..HEAD`: PASS
- `ctest --test-dir build-ralph --output-on-failure`: FAIL, 102/103 passed; only `skygate-ui-qml-main-window-tests` failed in `QmlMainWindowTests::footerPopupToolbarToggleClickClosesPopupAndTogglesToolbar()`, matching the existing HP-051 tracked regression.

# Regression risk

Low

The change is source-compatible, leaves the existing `SkyContext` compute path intact, and exercises the new request snapshot path through focused tests. The remaining full-suite failure is outside this task and already tracked as HP-051.

# Out-of-scope observations

- The verifier prompt references `specs/high-precision-ephemeris-engine.md`, but this checkout contains `spec/high-precision-ephemeris-engine.md`.
- Request overloads for body lookup and broader fake/test engine migration remain scoped to later HP-006 child tasks.

# Final recommendation

PASS: ready for final acceptance or merge.
