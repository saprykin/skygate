## Verdict

NEEDS_FIX

## Task reviewed

- ID: HP-027F
- Title: Record applied correction flags, warnings, and per-flag tests
- Source: IMPLEMENTATION_PLAN.md HP-027F
- Base ref: 832e1a937d478f628b9fc1df49015e2f10f9d133
- Head ref: 0b381500c4bdcc6dbc86b3e91c764ee4694e1a3d

## Summary

The implementation adds requested, skipped, and unavailable correction metadata,
and finalizes correction tracking in the simple and high-precision result
builders. Most paths use the new helper consistently, but several edge cases
still produce contradictory or incomplete metadata for requested corrections.

## Checks performed

- [x] Read active task
- [x] Read implementation handoff
- [x] Read relevant specs
- [x] Inspected git diff
- [x] Inspected relevant tests
- [x] Ran relevant tests, if safe/applicable

## Findings

### Finding 1: Missing observer omits correction warning

Severity: MAJOR
File: `libs/skygate-ephemeris/src/engine/highprecision/ApparentPlaceCalculator.cpp`
Lines/functions: 337-342, `ApparentPlaceCalculator::apply`

Problem:
When a topocentric request has no valid observer, the code marks
`DiurnalParallax` in `unavailableCorrections` by OR-ing the bit directly. That
bypasses `addUnavailableCorrection()`, so the result gets `MissingObserver` but
does not get the stable `CorrectionUnavailable` warning required for a
requested-but-unavailable correction.

Why it matters:
Callers can see that the correction is unavailable only if they inspect the new
bit field. Existing warning-driven UI or diagnostics will silently miss that a
requested correction could not be applied.

Recommended fix:
Use `addUnavailableCorrection(EphemerisCorrectionFlags::DiurnalParallax)` in
this branch, while preserving the existing `MissingObserver` warning and
degraded status. Add a test that asserts both warning metadata and the
unavailable bit for the invalid-observer topocentric path.

### Finding 2: Partial proper motion is both applied and unavailable

Severity: MAJOR
File: `libs/skygate-ephemeris/src/engine/highprecision/StarAstrometryCalculator.cpp`
Lines/functions: 330-358, `recordUnavailableRequestedFields`,
`recordAppliedCorrections`

Problem:
For a star with only one proper-motion component,
`recordUnavailableRequestedFields` marks `ProperMotion` unavailable because one
component is missing, while `recordAppliedCorrections` marks the same
`ProperMotion` flag applied because one component exists. A single correction
flag can therefore appear in both `appliedCorrections` and
`unavailableCorrections`.

Why it matters:
HP-027F requires requested, applied, skipped, and unavailable corrections to be
distinguished. Reporting the same flag as both applied and unavailable makes the
metadata ambiguous and can cause callers to overstate correction quality.

Recommended fix:
Choose one consistent per-flag state for partial proper-motion data. If both
components are required for the correction to count as applied, only mark
`ProperMotion` applied when both are present. If partial application is
intended, add a distinct warning/status model for partial corrections instead
of putting the same flag in both buckets. Add a regression test for one missing
proper-motion component.

### Finding 3: Refraction-only simple requests lose unavailable flag detail

Severity: MAJOR
File: `libs/skygate-ephemeris/src/engine/simple/SimpleEphemerisEngine.cpp`
Lines/functions: 75-92, `requestsUnsupportedSimpleOptions`,
`markUnsupportedSimpleOptions`

Problem:
`enableAtmosphericRefraction=true` makes a simple-engine request unsupported
even when `correctionFlags == NoCorrections`, but the unavailable correction
recorded is `options.correctionFlags`. In that case the result can carry a
`CorrectionUnavailable` warning while `requestedCorrections`,
`skippedCorrections`, and `unavailableCorrections` are all `NoCorrections`.

Why it matters:
The warning no longer identifies which requested option was unavailable, so the
new correction metadata does not explain the degradation. This is especially
confusing because atmospheric refraction is controlled by both a boolean option
and a correction flag.

Recommended fix:
Only treat atmospheric refraction as a requested unavailable correction when
the `AtmosphericRefraction` flag is present, or fold the enabled refraction
option into the requested correction set before finalizing metadata. Add a
simple-engine regression test for `enableAtmosphericRefraction=true` with
`NoCorrections`.

## Test assessment

I ran:

- relevant ephemeris subset with `ctest --test-dir build-ralph
  --output-on-failure -R <ephemeris metadata tests>`
- `ctest --test-dir build-ralph --output-on-failure`

The full configured suite passed: 120 tests passed and 2 CALCEPH-gated tests
were skipped. The skipped tests were
`skygate-ephemeris-calceph-kernel-provider-tests` and
`skygate-ephemeris-solar-system-state-calculator-tests`.

The added tests cover core finalization and several unavailable paths, but they
miss the invalid-observer diurnal parallax warning, partial proper-motion
metadata, and the simple-engine refraction-only edge case.

## Regression risk

Medium

The new fields are public metadata and the incorrect states occur on edge
paths, but those paths are exactly what HP-027F is meant to make visible and
stable for callers.

## Out-of-scope observations

No out-of-scope observations.

## Final recommendation

NEEDS_FIX: fix listed findings, then rerun review.
