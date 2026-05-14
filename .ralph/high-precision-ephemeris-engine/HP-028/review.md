## Verdict

NEEDS_FIX

## Task reviewed

- ID: HP-028
- Title: Implement topocentric observer and diurnal parallax pipeline
- Source: Spec: Goals; Correction Options; Testing Requirements
- Base ref: f1b4e7b9a78db65790167346b2c3872bf0859b67
- Head ref: 46a52ff3020a7a6db5c5e0c8dbed457cd9491033

## Summary

The implementation carries solar-system observer-relative AU vectors through the
calculator result, applies a WGS84 observer offset in the topocentric ITRS path,
and adds focused unit tests for parallax, invalid observer, and missing distance
vectors. The core direction is consistent with HP-028, and the current test
suite passes, but the required Horizons topocentric fixture validation and
several requested edge-case tests are still missing.

## Checks performed

- [x] Read active task
- [x] Read implementation handoff
- [x] Read relevant specs
- [x] Inspected git diff
- [x] Inspected relevant tests
- [x] Ran relevant tests, if safe/applicable

## Findings

### Finding 1: Required topocentric validation coverage is missing

Severity: MAJOR
File: `libs/skygate-ephemeris/tests/highprecision/ApparentPlaceCalculatorTests.cpp`
Lines/functions: `appliesTopocentricParallaxAndHorizontalCoordinates`, `reportsInvalidObserverForTopocentricRequest`, `reportsUnavailableParallaxWhenDistanceVectorIsMissing`

Problem:
HP-028 requires topocentric Moon, Sun, and planet fixture tests against Horizons
observer/apparent quantities, plus tests for missing EOP data, parallax
enabled/disabled, and elevation effects. The new coverage is limited to
synthetic pass-through frame-transformer unit tests and does not add any
topocentric Horizons fixtures under `libs/skygate-ephemeris/tests/fixtures/ephemeris`.
There is also no direct test that elevation changes the observer offset, no
explicit parallax-disabled comparison, and no explicit missing-EOP topocentric
case.

Why it matters:
This code is a numerical pipeline that depends on frame transforms, Earth
orientation, observer geodetic conversion, and Horizons-compatible apparent
topocentric semantics. Pass-through unit tests can prove local control flow, but
they cannot validate that the composed real pipeline produces correct
observer/apparent Moon, Sun, or planet coordinates. Without those fixtures, the
task acceptance criteria are not actually demonstrated.

Recommended fix:
Add deterministic topocentric Horizons fixture coverage for at least Moon, Sun,
and one planet using observer/apparent quantities. Exercise the real
`ErfaFrameTransformer` and time/EOP plumbing rather than only a pass-through
transformer. Add focused edge-case tests for missing EOP data, parallax
enabled/disabled comparison, and elevation changing the resulting topocentric
position or horizontal coordinates.

## Test assessment

Existing relevant tests were built and run:

- `cmake --build build-ralph --target skygate-ephemeris-apparent-place-calculator-tests skygate-ephemeris-solar-system-state-calculator-tests`
- `ctest --test-dir build-ralph -R 'skygate-ephemeris-(apparent-place-calculator|solar-system-state-calculator)-tests' --output-on-failure`
- `ctest --test-dir build-ralph --output-on-failure`

All 61 tests in `build-ralph` passed. The new tests cover local topocentric
control flow, but they do not satisfy the required topocentric Horizons fixture
validation or all requested HP-028 edge cases.

## Regression risk

Medium

The change touches the high-precision apparent-place path and is isolated from
the simple engine, but the unvalidated real topocentric numerical pipeline can
produce incorrect observer/apparent coordinates without being caught by the
current synthetic tests.

## Out-of-scope observations

None.

## Final recommendation

NEEDS_FIX: fix listed findings, then rerun review.
