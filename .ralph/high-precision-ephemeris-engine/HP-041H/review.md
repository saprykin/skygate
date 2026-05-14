## Verdict

NEEDS_FIX

## Task reviewed

- ID: HP-041H
- Title: Add selected-engine integration test matrix
- Source: IMPLEMENTATION_PLAN.md
- Base ref: e40c5571374d6a1f7acef56a8846baff23712419
- Head ref: 94e6fe843f5f45c549499758c1a7b1f55e63f2e6

## Summary

The implementation adds a new Qt integration test target with fake Simple and
HighPrecision engines and verifies several selected-engine request paths. The
test builds and the full suite passes, but several claimed acceptance criteria
are only checked for non-empty output or metadata, not for selected-engine or
option-sensitive behavior. The matrix needs stronger assertions before this
task can be considered complete.

## Checks performed

- [x] Read active task
- [x] Read implementation handoff
- [x] Read relevant specs
- [x] Inspected git diff
- [x] Inspected relevant tests
- [x] Ran relevant tests, if safe/applicable

## Findings

### Finding 1: Matrix does not prove several consumers switch

Severity: MAJOR
File: `apps/skygate-ui/tests/app/SkyContextControllerSelectedEngineMatrixTests.cpp`
Lines/functions: lines 352, 375-378, 397-405,
`verifyConsumerMatrix`

Problem:
Several required consumers are asserted only through non-empty or generic
checks. Render coverage is `renderPointSpan().empty()` only. Event coverage
only checks that Rise, Set, and Culmination strings are non-empty. Trail
coverage only checks that `renderLineSpan()` is non-empty, even though that span
can include non-trail line sources. Night conditions accept any icon in
`{sun, twilight, moon}` and only require a valid payload. These checks can pass
without proving that the rendered positions, trail samples, event values, or
night-condition summaries changed with engine kind or correction options.

Why it matters:
HP-041H specifically asks for a selected-engine integration matrix proving
render, trails, night conditions, and event calculations switch together. The
current test could miss regressions where those surfaces still render something
but ignore selected high-precision options or use stale/simple-engine data.

Recommended fix:
Strengthen the matrix assertions so each claimed consumer observes the fake
engine's distinct coordinates or option tier. For example, compare render point
positions/body states, compare trail line output across option tiers, compare
Rise/Set/Culmination or night-condition summary values between Simple,
HighPrecision without LightTime, and HighPrecision with LightTime, and assert
request counters/options after each isolated consumer action.

### Finding 2: Applicable reference overlays are not exercised

Severity: MAJOR
File: `apps/skygate-ui/tests/app/SkyContextControllerSelectedEngineMatrixTests.cpp`
Lines/functions: lines 352-358, `verifyConsumerMatrix`

Problem:
The test reads `sceneModel.referenceOverlayContext()` and checks the snapshot
context longitude, but it never enables or inspects the actual reference overlay
layers. In production, ecliptic, celestial-equator, and circumpolar labels are
only built when their overlay layer flags are enabled, and those flags default
to false. As written, the matrix does not exercise the overlay rendering paths
that HP-041H calls out as applicable reference overlays.

Why it matters:
A regression in the actual reference overlay layer rendering could pass this
matrix as long as the cached reference context exists. That leaves part of the
task's acceptance criteria uncovered.

Recommended fix:
Enable the applicable reference overlay layers in the harness and assert their
overlay output changes or is positioned consistently with the selected snapshot
context for both engine kinds and relevant option tiers.

## Test assessment

The new CTest target is registered through the existing UI Qt-test helper and
builds cleanly. I ran:

- `cmake --build build-ralph --target
  skygate-ui-context-controller-selected-engine-matrix-tests`
- `ctest --test-dir build-ralph --output-on-failure -R
  skygate-ui-context-controller-selected-engine-matrix-tests`
- `ctest --test-dir build-ralph --output-on-failure`

The targeted test passed, and the full suite passed 125/125 with the two
existing CALCEPH-dependent tests skipped by their configured skip rules. The
main gap is assertion depth, not test registration or runtime stability.

## Regression risk

Medium

The change is test-only, so production regression risk from the patch itself is
low. The acceptance risk is medium because the test can pass while several
selected-engine integration paths remain insufficiently verified.

## Out-of-scope observations

No out-of-scope observations.

## Final recommendation

NEEDS_FIX: fix listed findings, then rerun review.
