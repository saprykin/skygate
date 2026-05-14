## Verdict

NEEDS_FIX

## Task reviewed

- ID: HP-041B
- Title: Apply selected engine and request to frame rendering
- Source: IMPLEMENTATION_PLAN.md
- Base ref: 8a2739b7d9cfcb0902a4d8c2c522567481ade9fa
- Head ref: 09b8a35fd6ff5286b660484c63118b250eb38820

## Summary

The implementation wires `SkySceneModel` to build frame input from the app-level
ephemeris request context, and `SkySceneFramePipeline` now dispatches snapshot
computation through `IEphemerisEngine::compute(const EphemerisRequest&)` when a
request is available. However, request option changes can be hidden by the
scene snapshot cache, and the added test changes the engine pointer at the same
time as the options. That does not satisfy the task verification that request
options affect rendered frame results.

## Checks performed

- [x] Read active task
- [x] Read implementation handoff
- [x] Read relevant specs
- [x] Inspected git diff
- [x] Inspected relevant tests
- [x] Ran relevant tests, if safe/applicable

## Findings

### Finding 1: Request option changes can reuse stale frame snapshots

Severity: MAJOR
File: `apps/skygate-ui/src/scene/SkySceneFramePipeline.cpp`
Lines/functions: `SkySceneFramePipeline::rebuild`, lines 41-50

Problem:
The pipeline builds its snapshot cache key from catalog revision, observer, and
UTC time, then skips recomputation when that key and the engine pointer match.
When `ephemerisRequest` is present, the actual compute call uses the full
request, including `EphemerisRequest::options` and `epoch`, but those request
fields are not part of the invalidation decision. A correction flag,
refraction, fallback-policy, or explicit epoch change with the same engine
instance, time, observer, and catalog revision will keep rendering the old
snapshot.

The new test does not catch this because it changes both request options and
`input.ephemerisEngine` before the second rebuild. The existing engine-pointer
check is enough to make that pass even if request options are otherwise stale.

Why it matters:
HP-041B requires frame rendering to use the selected engine/request and to
switch when request options change. With the current cache behavior, app-level
request settings can fail to affect frame coordinates until some unrelated
snapshot key field changes.

Recommended fix:
Ensure frame snapshot invalidation changes when the request fields used for
frame rendering change, without taking over the full HP-042 cache-key expansion.
One acceptable narrow fix is to pass a request/options revision or other
minimal request fingerprint into the frame pipeline and include it in the
snapshot invalidation check. Add a test that changes request options on the
same engine instance and proves the snapshot and rendered frame recompute.

## Test assessment

`skygate-ui-sky-scene-frame-pipeline-tests` now verifies request API dispatch
when an `EphemerisRequest` is supplied and verifies that distinct fake engines
can produce distinct snapshot coordinates. It does not verify option-only
changes on the same engine, and it asserts raw snapshot horizontal coordinates
rather than projected `frame.points`.

Commands run:

- `cmake --build build-ralph --target skygate-ui-sky-scene-frame-pipeline-tests`
- `ctest --test-dir build-ralph -R skygate-ui-sky-scene-frame-pipeline-tests
  --output-on-failure`
- `ctest --test-dir build-ralph --output-on-failure`

The full configured suite passed 124/124 tests. The existing CALCEPH kernel
provider and solar-system state calculator tests were skipped by configuration.

## Regression risk

Medium

The implementation touches the frame-rendering path and mostly preserves the
existing cache boundary. The remaining risk is stale rendered positions when a
request-only field changes before HP-042 expands the complete scene cache key.

## Out-of-scope observations

- Trails still use legacy `SkyContext` sampling; HP-041E appears to own that.
- Inspector and observation event calculations still use legacy `SkyContext`;
  HP-041D appears to own that.

## Final recommendation

NEEDS_FIX: fix listed findings, then rerun review.
