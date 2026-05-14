# Verdict

PASS

# Task verified

- ID: HP-041B
- Title: Apply selected engine and request to frame rendering
- Source: IMPLEMENTATION_PLAN.md
- Base ref: 8a2739b7d9cfcb0902a4d8c2c522567481ade9fa
- Head ref: 0ca3ee393563c5cf4b89cdfc688e55ae7a1beb53

# Summary

Frame rendering now receives the selected ephemeris engine and the app-level
`EphemerisRequest` from `SkyContextController`, and the frame pipeline dispatches
snapshot computation through the request-based engine API when a request is
present. The review finding about stale snapshots after request-only changes was
fixed by including request epoch and engine options in the snapshot invalidation
key. The focused regression test and full configured test suite pass, so this
task is ready for acceptance.

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

- Finding: Request option changes can reuse stale frame snapshots
  - Original severity: MAJOR
  - Closure status: Resolved
  - Notes: `SkySceneFramePipeline` now stores request epoch and options in the
    snapshot cache key whenever request-based rendering is active. The added
    same-engine regression test proves option-only changes recompute both the
    snapshot and render frame.

# Findings

No findings.

# Test assessment

Relevant coverage exists in
`apps/skygate-ui/tests/scene/SkySceneFramePipelineTests.cpp`. It verifies
request-based dispatch, selected simple versus high-precision options, and
same-engine request option changes that update rendered point coordinates.

Tests run:

- `ctest --test-dir build-ralph -R
  skygate-ui-sky-scene-frame-pipeline-tests --output-on-failure`: PASS
- `ctest --test-dir build-ralph --output-on-failure`: PASS, 124/124 configured
  tests passed

The configured suite skipped
`skygate-ephemeris-calceph-kernel-provider-tests` and
`skygate-ephemeris-solar-system-state-calculator-tests`, consistent with the
current build configuration.

# Regression risk

Low

The changes are limited to frame rendering input construction, request-based
snapshot computation, and snapshot invalidation for request fields consumed by
the frame pipeline. The full configured suite passes.

# Out-of-scope observations

- Search focus and tracking request routing remain owned by HP-041C.
- Inspector and observation event request routing remain owned by HP-041D.
- Trail request routing remains owned by HP-041E.

# Final recommendation

PASS: ready for final acceptance or merge.
