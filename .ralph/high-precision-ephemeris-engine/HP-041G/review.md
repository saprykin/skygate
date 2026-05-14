## Verdict

NEEDS_FIX

## Task reviewed

- ID: HP-041G
- Title: Reconcile observer/time-dependent reference overlays
- Source: IMPLEMENTATION_PLAN.md
- Base ref: fc4b3d5ad477a2058f7fc6a09aefaf7ab600b785
- Head ref: 84377ee209e618a62cbf749b6d8f49823fc5b3ce

## Summary

The implementation routes scene-graph reference line rendering through the
resolved scene snapshot context and adds tests around that context exposure.
However, reference overlay labels still use the pre-compute frame input context,
so labels can be computed in a different observer/time frame from the reference
lines they annotate. This leaves the task incomplete.

## Checks performed

- [x] Read active task
- [x] Read implementation handoff
- [x] Read relevant specs
- [x] Inspected git diff
- [x] Inspected relevant tests
- [x] Ran relevant tests, if safe/applicable

## Findings

### Finding 1: Reference labels ignore resolved snapshot context

Severity: MAJOR
File: `apps/skygate-ui/src/scene/SkySceneComposition.cpp`
Lines/functions: 179-223, `SkySceneComposer::buildOverlayItems`

Problem:
`buildOverlayItems()` binds `skyContext` to `input.frameInput.skyContext` and
uses it for the ecliptic, celestial-equator, and circumpolar label positions.
The HP-041G implementation changed scene-graph reference lines to use
`SkySceneModel::referenceOverlayContext()`, which comes from
`sceneFrame.snapshot->context`, but the label path did not get the same
treatment.

Why it matters:
When the selected engine/request resolves a different snapshot context than the
controller input, the rendered reference lines and their labels are computed
from different observer/time values. That violates the task goal to reconcile
observer/time-dependent overlays with the selected engine/request context, and
it can visibly detach or misplace the labels for those reference overlays.

Recommended fix:
Use the scene snapshot context for reference label calculations when available,
for example by deriving the label context from `sceneFrame.snapshot->context`
instead of `input.frameInput.skyContext`. Add or adjust a composition test that
passes a `SkySceneFramePipelineResult` snapshot with a deliberately different
context from `input.frameInput.skyContext` and asserts the label is placed only
when centered on the snapshot-context reference coordinate.

## Test assessment

New tests cover the exposed snapshot context on `SkySceneModel` and document
that pure reference-label math does not require an engine lookup. They do not
cover the failing case where `frameResult.snapshot->context` differs from
`input.frameInput.skyContext`, so the label/line context mismatch is not caught.

I ran:

- `ctest --test-dir build-ralph --output-on-failure -R
  'skygate-ui-(sky-scene-composition|scene-model-frame)-tests'`
- `ctest --test-dir build-ralph --output-on-failure`

The full suite passed: 124 tests, with CALCEPH-dependent tests 33 and 34
skipped by the configured skip conditions.

## Regression risk

Medium

The change is localized to reference overlays, but the current mismatch affects
visible UI annotation whenever an engine returns a resolved context that differs
from the request input.

## Out-of-scope observations

None.

## Final recommendation

NEEDS_FIX: fix listed findings, then rerun review.
