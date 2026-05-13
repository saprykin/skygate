## Verdict

NEEDS_FIX

## Task reviewed

- ID: HP-027B
- Title: Implement solar-system light-time correction
- Source: IMPLEMENTATION_PLAN.md; specs/high-precision-ephemeris-engine.md
- Base ref: ca46d8df3fafb9aecf04e518820b1912432aea4f
- Head ref: d02d6316dfd840f7eaaf6f7439452485eeb1502c

## Summary

The implementation adds correction-flag-controlled solar-system light-time
handling in `SolarSystemStateCalculator`, using receive-time Earth state and
iterated target transmit-time state, and preserves the geometric fallback when
required light-time inputs are unavailable. The core structure is reasonable and
the current test suite passes, but the task's required fixture/reference
coverage for representative light-time-corrected major-body output is missing.

## Checks performed

- [x] Read active task
- [x] Read implementation handoff
- [x] Read relevant specs
- [x] Inspected git diff
- [x] Inspected relevant tests
- [x] Ran relevant tests, if safe/applicable

## Findings

### Finding 1: Missing reference coverage for light-time-corrected output

Severity: MAJOR
File: `libs/skygate-ephemeris/tests/highprecision/SolarSystemStateCalculatorTests.cpp`
Lines/functions: `appliesLightTimeCorrectionFromRetardedTargetAndReceiveEarth`

Problem:
HP-027B explicitly requires fixture or reference tests for representative
supported major bodies. The added light-time test uses a synthetic fake provider
that returns the same target barycentric vector for every retarded epoch, so it
only verifies call shape and a constructed RA/Dec. It does not compare
light-time-enabled output against an external fixture or reference value for a
real supported major body.

Why it matters:
This is a numerical feature, and the current tests would pass if the iteration
count, convergence behavior, or target transmit-time values produced a
physically wrong astrometric direction as long as the fake provider returned the
same canned vector. That leaves the acceptance criteria only structurally
tested, not numerically validated.

Recommended fix:
Add a compact fixture/reference test for at least one representative supported
major body with light-time enabled. The fixture should include receive epoch,
receive-time Earth state, target states at the expected retarded epochs or a
trusted final astrometric RA/Dec/vector, and assert the corrected output within
a documented tolerance. A Horizons-derived ICRF/no-apparent-correction fixture
would match the surrounding specification.

## Test assessment

The implementation adds tests for light-time enabled behavior and for
receive-time Earth-state unavailability preserving the geometric fallback. It
does not add the required reference/fixture validation for a representative
light-time-corrected major body. It also does not directly exercise the
retarded target-state unavailable branch, but that is a smaller branch coverage
gap than the missing numerical reference test.

Tests run:
- `cmake --build build-ralph --target skygate-ephemeris-solar-system-state-calculator-tests`
- `ctest --test-dir build-ralph -R skygate-ephemeris-solar-system-state-calculator-tests --output-on-failure`
- `ctest --test-dir build-ralph --output-on-failure`
- `git diff --check ca46d8df3fafb9aecf04e518820b1912432aea4f..HEAD`

All executed tests passed.

## Regression risk

Medium

The code path is well scoped and falls back cleanly when extra inputs are
missing, but the numerical behavior is not independently validated for a real
light-time-corrected major-body case.

## Out-of-scope observations

None.

## Final recommendation

NEEDS_FIX: fix listed findings, then rerun review.
