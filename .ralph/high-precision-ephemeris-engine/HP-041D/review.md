## Verdict

NEEDS_FIX

## Task reviewed

- ID: HP-041D
- Title: Apply selected engine and request to inspector and observation events
- Source: IMPLEMENTATION_PLAN.md
- Base ref: 222ddc9b2676576a87efec7652a5e7f69601ffd3
- Head ref: f3b9214919b5c8ac11e3d0c3faa09f2f29c0d024

## Summary

The implementation routes inspector observation events through
`EphemerisRequest` when the scene pipeline provides one, and adds tests for
request option propagation and sampled epoch updates. The request overloads are
useful, but the existing context-based observation event overloads now create a
fresh default request instead of preserving the selected engine's configured
options. That is a compatibility regression and should be fixed before final
verification.

## Checks performed

- [x] Read active task
- [x] Read implementation handoff
- [x] Read relevant specs
- [x] Inspected git diff
- [x] Inspected relevant tests
- [x] Ran relevant tests, if safe/applicable

## Findings

### Finding 1: Context event overloads drop engine options

Severity: MAJOR
File: `libs/skygate-ephemeris/src/ObservationEventCalculator.cpp`
Lines/functions: 44-49, 336-374, `requestFromContext`,
`ObservationEventCalculator::compute`

Problem:
The context-based `ObservationEventCalculator::compute` overloads now call
`requestFromContext(context)`. That helper fills only `context` and `epoch`;
it does not copy `ephemerisEngine.options()`. Before this change, these
overloads sampled via `ephemerisEngine.computeBodyState(context, ...)`, which
let each engine build its own compatibility request using configured options.

Why it matters:
Callers that still use the public context overloads can now get default request
options instead of the selected engine options. That can change high-precision
correction, refraction, atmosphere, fallback, or engine-kind behavior. This
also affects current context-based users such as night conditions until they
are migrated to explicit requests.

Recommended fix:
Seed compatibility requests from the engine, for example by passing the engine
to `requestFromContext` and assigning `request.options =
ephemerisEngine.options()`. Alternatively, keep the context overloads on the
old context-based sampling path and reserve request sampling for explicit
`EphemerisRequest` callers. Add a regression test with a context-only caller
and an engine whose `options()` differs from default request options.

## Test assessment

Relevant tests were added in
`libs/skygate-ephemeris/tests/events/ObservationEventCalculatorTests.cpp` and
`apps/skygate-ui/tests/selection/SkySelectionOverlayBuilderTests.cpp`. They
cover explicit request option propagation, sampled epoch updates, and inspector
event summaries using request-sensitive behavior.

I built and ran the targeted tests:
`skygate-ephemeris-observation-event-calculator-tests` and
`skygate-ui-sky-selection-overlay-builder-tests`; both passed. I also ran the
full suite with `ctest --test-dir build-ralph --output-on-failure`: 122 tests
passed and 2 high-precision dependency tests were skipped.

Missing coverage: no test currently proves the context-based observation event
overloads preserve engine-configured options.

## Regression risk

Medium

The main request-based inspector path is covered, but the public context
overloads are still used by existing code and now route through default request
options.

## Out-of-scope observations

The full HP-041D integration intent mentions inspector coordinate switching.
Coordinates currently come from the request-built frame snapshot, so this is
mostly covered by earlier frame-routing work, but an end-to-end inspector test
would make that relationship clearer.

## Final recommendation

NEEDS_FIX: fix listed findings, then rerun review.
