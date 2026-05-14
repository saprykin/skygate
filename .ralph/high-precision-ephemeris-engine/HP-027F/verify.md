# Verdict

PASS

# Task verified

- ID: HP-027F
- Title: Record applied correction flags, warnings, and per-flag tests
- Source: IMPLEMENTATION_PLAN.md HP-027F
- Base ref: 832e1a937d478f628b9fc1df49015e2f10f9d133
- Head ref: 8ed5630

# Summary

The implementation adds public correction metadata for requested, applied,
skipped, and unavailable corrections, records stable
`CorrectionUnavailable` warnings, and finalizes correction tracking in the
simple and high-precision result paths. The review found three metadata edge
cases, and the fix pass addressed each with focused code changes and
regression tests. I verified the fixes, inspected the relevant diff and tests,
and ran the relevant subset plus the full configured suite. The task is ready
for acceptance.

# Checks performed

- [x] Read active task
- [x] Read implementation handoff
- [x] Read review report
- [x] Read fixer report, if present
- [x] Read relevant specs
- [x] Inspected git history
- [x] Inspected git diff
- [x] Inspected relevant tests
- [x] Ran relevant tests, if safe/applicable

# Review/fix closure

- Finding: Missing observer omits correction warning
  - Original severity: MAJOR
  - Closure status: Resolved
  - Notes: Invalid topocentric observers now record unavailable
    `DiurnalParallax` through `addUnavailableCorrection()`, preserving
    `MissingObserver` and adding stable `CorrectionUnavailable` metadata.

- Finding: Partial proper motion is both applied and unavailable
  - Original severity: MAJOR
  - Closure status: Resolved
  - Notes: `ProperMotion` is now marked applied only when both proper-motion
    components are present. Partial astrometry remains degraded and
    unavailable, with regression coverage.

- Finding: Refraction-only simple requests lose unavailable flag detail
  - Original severity: MAJOR
  - Closure status: Resolved
  - Notes: Simple-engine requests no longer treat
    `enableAtmosphericRefraction=true` as an unsupported requested correction
    when `correctionFlags` is `NoCorrections`.

# Findings

No findings.

# Test assessment

Relevant tests cover public metadata finalization, high-precision result
builder finalization, simple-engine unsupported correction metadata,
apparent-place topocentric/refraction unavailable paths, atmospheric refraction,
solar-system correction flags, and star-astrometry correction flags.

I ran:

- `cmake --build build-ralph --target
  skygate-ephemeris-apparent-place-calculator-tests
  skygate-ephemeris-star-astrometry-calculator-tests
  skygate-ephemeris-engine-baseline-tests`
- `ctest --test-dir build-ralph --output-on-failure -R
  'apparent-place|star-astrometry|engine-baseline'`
- `ctest --test-dir build-ralph --output-on-failure`

The targeted subset passed. The full configured suite passed with 120 tests
passed and 2 CALCEPH-gated tests skipped:
`skygate-ephemeris-calceph-kernel-provider-tests` and
`skygate-ephemeris-solar-system-state-calculator-tests`.

# Regression risk

Low

The changes are localized to metadata bookkeeping and edge-case handling. The
full configured suite passes, and review-specific regressions were covered by
tests.

# Out-of-scope observations

- Some metadata tests assert warning presence but not every warning text string
  directly. This does not block HP-027F because the public warning text exists
  and the relevant warning code is exercised.
- Partial proper-motion source data is still numerically treated as a fallback
  using the present component and zero for the missing component, while metadata
  reports the correction as unavailable. That is a semantic modeling question
  for a future task, not a blocker for the reviewed metadata ambiguity.

# Final recommendation

PASS: ready for final acceptance or merge.
