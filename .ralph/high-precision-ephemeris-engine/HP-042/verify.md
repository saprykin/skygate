# Verdict

PASS

# Task verified

- ID: HP-042
- Title: Extend scene and app cache keys for high precision
- Source: `IMPLEMENTATION_PLAN.md`
- Base ref: fbce8ae80f5cd301b017b0019e2a32e861524f0b
- Head ref: 513dbb71ff88f6c09c63e5f2cc1014016f16f48b

# Summary

HP-042 extends the scene snapshot cache inputs with high-precision engine
context: engine kind, option revision, ephemeris data revision, EOP revision,
leap-second revision, astronomical epoch, request options, observer, and time.
The review report passed with no findings, and the fix pass correctly made no
source changes. The focused scene/controller tests and full configured
`build-ralph` suite pass, so the task is ready for acceptance.

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

The implementation adds `SkySceneFramePipeline` coverage for engine kind,
engine option revision, ephemeris data revision, EOP revision, leap-second
revision, subsecond astronomical epoch changes, and render-only changes that
must not recompute ephemeris snapshots. Existing controller and scene-model
tests cover request-context wiring from restored engine settings and selected
engine state.

Tests run:

```sh
ctest --test-dir build-ralph -R skygate-ui-sky-scene-frame-pipeline-tests \
  --output-on-failure
ctest --test-dir build-ralph \
  -R skygate-ui-context-controller-ephemeris-settings-tests \
  --output-on-failure
ctest --test-dir build-ralph \
  -R skygate-ui-context-controller-selected-engine-matrix-tests \
  --output-on-failure
ctest --test-dir build-ralph -R skygate-ui-scene-model-frame-tests \
  --output-on-failure
ctest --test-dir build-ralph --output-on-failure
```

The focused tests passed. The full configured suite passed with 123 tests run
successfully and the two existing CALCEPH-dependent tests skipped.

# Regression risk

Low

The source changes are localized to request-context propagation and cache-key
construction. Render-frame cache keys remain separate, and tests verify
render-only changes do not trigger ephemeris recomputation.

# Out-of-scope observations

- App-level `core::UtcTimePoint` remains second-resolution, but HP-042's
  request-based astronomical epoch path preserves subsecond cache-key changes
  when an `EphemerisRequest` supplies them.

# Final recommendation

PASS: ready for final acceptance or merge.
