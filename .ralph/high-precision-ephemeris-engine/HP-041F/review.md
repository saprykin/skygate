## Verdict

NEEDS_FIX

## Task reviewed

- ID: HP-041F
- Title: Apply selected engine and request to night conditions
- Source: IMPLEMENTATION_PLAN.md
- Base ref: c855c1030e4eac6ab33b9bca3d66af212a199bb5
- Head ref: 4851cd748e2f721c3c669b2db7e9cba94a57a216

## Summary

The implementation routes the night-conditions icon and popup refresh through
the request-based engine APIs, and the existing full test suite passes in
`build-ralph`. The main behavior is close, but the added integration test does
not prove that controller-selected request options are used. The new request
overload also still derives Moon phase from `request.context.utcTime`, which can
mix epochs when callers provide an explicit request epoch.

## Checks performed

- [x] Read active task
- [x] Read implementation handoff
- [x] Read relevant specs
- [x] Inspected git diff
- [x] Inspected relevant tests
- [x] Ran relevant tests, if safe/applicable

## Findings

### Finding 1: Request overload mixes request epoch and context time

Severity: MAJOR
File: `libs/skygate-ephemeris/src/NightConditionsCalculator.cpp`
Lines/functions: `NightConditionsCalculator::compute(const EphemerisRequest&)`

Problem:
The request overload computes Sun/Moon states and rise/set summaries through
the request-based engine path, but Moon phase and illumination are still derived
from `request.context.utcTime` at line 142. If an `EphemerisRequest` carries an
epoch that differs from `context.utcTime`, or uses a non-UTC time scale, the
night-conditions result can combine positions/events for one epoch with Moon
phase data for another.

Why it matters:
HP-041F is specifically about applying the selected ephemeris request context to
night conditions. Mixing the explicit request epoch with a separate context time
weakens that contract and makes the new overload fragile for future
high-precision callers.

Recommended fix:
Base the lunar-cycle timestamp on the request epoch used for the rest of the
calculation, or normalize/validate the request so `context.utcTime` and
`request.epoch` cannot diverge before computing night conditions. Add a focused
test where the request epoch and context UTC intentionally differ.

### Finding 2: Integration test does not prove selected request options

Severity: MAJOR
File: `apps/skygate-ui/tests/app/SkyContextControllerNightCatalogTests.cpp`
Lines/functions: `nightConditionsUseSelectedEngineRequest`

Problem:
The new test only proves that request-based engine methods are reached. The
fake engine varies its output from `request.context.utcTime`, and the assertion
checks that the legacy context overload was not called. Because
`NightConditionsCalculator::compute(engine, context, ...)` now internally wraps
the context into a request, a future regression from
`ephemerisRequestContext()` back to the context overload in
`SkyContextControllerNightConditions.cpp` would still satisfy this test. The
test also never verifies `request.options.engineKind`, correction flags, or
other selected request options.

Why it matters:
The task verification requires integration coverage proving night-condition
icon/state and summary values switch when engine kind or request options change.
The current test would not catch a regression that drops controller-selected
options while still using request-based methods.

Recommended fix:
Extend the fake engine to make Sun altitude and/or event samples depend on a
selected request option, such as `request.options.engineKind` or
`correctionFlags`, then drive the controller option change and assert both the
icon and summary values switch. Also assert the fake engine observed the
expected selected options in the requests it received.

## Test assessment

Relevant coverage was added in
`skygate-ui-context-controller-night-catalog-tests`, and existing
`skygate-ephemeris-night-conditions-calculator-tests` still pass. The new test
does not yet cover selected engine kind or correction-option propagation, and no
test covers a request whose explicit epoch differs from `context.utcTime`.

Commands run:

- `ctest --test-dir build-ralph --output-on-failure -R
  'skygate-(ephemeris-night-conditions-calculator-tests|
  ui-context-controller-night-catalog-tests)'`
- `ctest --test-dir build-ralph --output-on-failure`

The full suite passed: 124 tests run, 122 passed, and 2 CALCEPH-dependent tests
were skipped by the existing build configuration.

## Regression risk

Medium

The changed code is focused, but it touches user-visible night-condition
summary data and a public calculator overload. The main risk is subtle request
context drift rather than an immediate build or runtime failure.

## Out-of-scope observations

No out-of-scope observations.

## Final recommendation

NEEDS_FIX: fix listed findings, then rerun review.
