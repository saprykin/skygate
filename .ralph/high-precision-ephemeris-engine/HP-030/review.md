## Verdict

NEEDS_FIX

## Task reviewed

- ID: HP-030
- Title: Implement high-precision result assembly
- Source: IMPLEMENTATION_PLAN.md; specs/high-precision-ephemeris-engine.md
- Base ref: 7bc9608
- Head ref: 79bf01e

## Summary

The implementation extracts the default high-precision result assembly into `EphemerisResultBuilder`, preserves calculator metadata, and handles valid, degraded, out-of-range, unsupported, and failed states. The main behavior is reasonable and the focused/full test suites pass, but the HP-030 verification requirements are not fully covered: several required data-condition tests are only simulated by preloading warning bits into a fake calculator result.

## Checks performed

- [x] Read active task
- [x] Read implementation handoff
- [x] Read relevant specs
- [x] Inspected git diff
- [x] Inspected relevant tests
- [x] Ran relevant tests, if safe/applicable

## Findings

### Finding 1: Required data-condition coverage is only synthetic

Severity: MAJOR
File: `libs/skygate-ephemeris/tests/highprecision/HighPrecisionEphemerisEngineTests.cpp`
Lines/functions: `defaultResultBuilderTurnsOutOfRangeFallbackIntoDegradedResult`, `defaultResultBuilderPreservesDegradedDataWarnings`

Problem:
HP-030 explicitly requires verification for missing DE441, stale EOP data, stale leap-second data, ancient Delta T assumptions, and unsupported bodies. The added unsupported-body coverage is concrete, but the other required cases are represented only by manually constructing a `HighPrecisionCalculatorResult` with warning bits and provenance strings such as `"missing DE441 fallback"` or `"stale EOP, stale leap-second, Delta T fallback"`.

Why it matters:
These tests prove that `EphemerisResultBuilder` preserves already-populated metadata, but they do not prove that the real high-precision pipeline maps missing kernels, stale time/EOP data, or ancient Delta T assumptions into the metadata that HP-030 says must be assembled. A regression in those provider/calculator paths would still pass these tests.

Recommended fix:
Add targeted tests that exercise the real relevant collaborators or fakes at their interfaces: a missing long-range kernel/DE441 scenario that produces the expected out-of-range/degraded fallback metadata, stale EOP and stale leap-second inputs that propagate the expected warnings/status, and an ancient-date Delta T assumption case. Keep the existing builder preservation tests if useful, but do not count them as the required data-condition coverage.

## Test assessment

The implementation adds focused result-assembly tests for valid metadata, out-of-range fallback degraded status, out-of-range without fallback, failed without fallback, and degraded warnings. It also retains concrete unsupported-body dispatch coverage. Missing coverage remains for the real missing-DE441, stale EOP, stale leap-second, and ancient Delta T scenarios required by HP-030.

Tests run:
- `cmake --build build-ralph --target skygate-ephemeris-highprecision-engine-tests -j2` passed.
- `ctest --test-dir build-ralph --output-on-failure -R skygate-ephemeris-highprecision-engine-tests` passed.
- `ctest --test-dir build-ralph --output-on-failure` passed: 61/61 tests.

## Regression risk

Low

The production code change is small and mostly preserves prior behavior while moving the default builder into its own component. The remaining risk is test adequacy for required degradation scenarios, not an observed runtime failure in the touched code.

## Out-of-scope observations

- Existing high-precision `SkyContext` compatibility overloads construct UTC epochs, while the current solar-system calculator expects TDB. This was not introduced by HP-030 and should be tracked separately if not already covered.
- Existing high-precision dispatch prioritizes solar-system body type over `FixedEquatorial` source. This was not introduced by HP-030 and may deserve separate follow-up against fixed-coordinate override behavior.

## Final recommendation

NEEDS_FIX: fix listed findings, then rerun review.
