## Verdict

NEEDS_FIX

## Task reviewed

- ID: HP-012
- Title: Implement UTC, TAI, and TT conversion
- Source: `IMPLEMENTATION_PLAN.md`
- Base ref: b5db65098daca798556ba56228a1e73240ef7f31
- Head ref: 3fd481506a11addb7477e8184d5eec5bccb68d48

## Summary

The implementation adds a leap-second-backed `ITimeScaleService`, UTC/TAI/TT conversion paths, `CivilDateTime` handling for `23:59:60`, and focused Qt tests. The normal UTC-to-TAI/TT paths are covered and pass, but two edge cases violate the task's range/status and leap-second-label requirements. The implementation should be fixed before final verification.

## Checks performed

- [x] Read active task
- [x] Read implementation handoff
- [x] Read relevant specs
- [x] Inspected git diff
- [x] Inspected relevant tests
- [x] Ran relevant tests, if safe/applicable

## Findings

### Finding 1: Reverse conversions ignore leap-second table validity

Severity: MAJOR
File: `libs/skygate-ephemeris/src/engine/highprecision/TimeScaleService.cpp`
Lines/functions: `lookupTaiOffset`, lines 164-206; `LeapSecondTimeScaleService::convert`, lines 268-290

Problem:
`lookupUtcOffset()` checks `tableInfo.validityRange` and returns failed or degraded status for UTC epochs outside the leap-second table range. `lookupTaiOffset()` does not perform an equivalent range check. It walks the table and returns the last known offset for any later TAI epoch, so `TAI -> UTC` and `TT -> UTC` conversions after the table expiry can return `Valid` with stale data.

Why it matters:
HP-012 requires degraded or failed conversion status when the request is outside the leap-second table range and no permitted fallback applies. Returning a valid UTC result after the table expiry silently claims precision the data cannot support, and TT-to-UTC inherits the same issue through its TAI intermediate path.

Recommended fix:
Apply the same validity policy to reverse conversions. For TAI/TT inputs, check the corresponding table-supported TAI interval or derive and validate a UTC candidate before returning success. If fallback is not allowed, fail with `EpochOutsideLeapSecondTable`; if fallback is allowed, return degraded status and warnings. Add tests for out-of-range `TAI -> UTC` and `TT -> UTC`.

### Finding 2: Public civil-to-epoch conversion loses leap-second labels

Severity: MAJOR
File: `libs/skygate-ephemeris/include/skygate/ephemeris/Types.hpp`
Lines/functions: `astronomicalEpochFromCivilDateTime`, lines 183-203

Problem:
`isValidCivilDateTime()` now accepts UTC `23:59:60`, but `astronomicalEpochFromCivilDateTime()` converts that label to a normalized `AstronomicalEpoch` for the next day's `00:00:00` UTC. If a caller uses this public helper and then calls `TimeScaleService::convert(epoch, TimeScale::Tai)`, the service applies the post-leap offset and produces the TAI instant one second after the leap second. The special pre-leap-offset handling only exists in `convertCivilDateTime()`.

Why it matters:
HP-012 explicitly requires support for leap-second instants such as `23:59:60` through `CivilDateTime`. With the current public API, one valid CivilDateTime path gives the correct instant while another accepted path collapses the label and converts it incorrectly. This is fragile for downstream callers and tests only cover the service-specific path.

Recommended fix:
Make leap-second label handling unambiguous. Either prevent the generic civil-to-epoch helper from accepting `second == 60` and require `ITimeScaleService::convertCivilDateTime()` for leap-second labels, or add a representation/API that preserves the leap-second label until the conversion policy can apply the correct pre-leap offset. Add a test that covers the public helper behavior so callers cannot accidentally get the post-leap instant.

## Test assessment

`skygate-ephemeris-time-scale-service-tests` covers normal UTC dates, UTC leap-second boundaries, `23:59:60` through `convertCivilDateTime()`, table range boundaries for UTC inputs, and missing-table degraded fallback. The tests do not cover out-of-range `TAI -> UTC` or `TT -> UTC`, and they do not cover the public `CivilDateTime -> AstronomicalEpoch -> convert()` path for `23:59:60`.

Commands run:
- `cmake --build build-ralph --target skygate-ephemeris-time-scale-service-tests`
- `ctest --test-dir build-ralph -R skygate-ephemeris-time-scale-service-tests --output-on-failure`
- `ctest --test-dir build-ralph -L 'highprecision|unit' --output-on-failure`
- `ctest --test-dir build-ralph --output-on-failure`

All commands passed.

## Regression risk

Medium

The new API is isolated to the ephemeris library and the existing test suite passes, but incorrect leap-second status/instant handling can propagate into later high-precision frame and apparent-place calculations.

## Out-of-scope observations

- The review prompt references `specs/high-precision-ephemeris-engine.md`, but this checkout contains `spec/high-precision-ephemeris-engine.md`.

## Final recommendation

NEEDS_FIX: fix listed findings, then rerun review.
