# Verdict

PASS

# Task verified

- ID: HP-027A
- Title: Add `ApparentPlaceCalculator` boundary and request mode routing
- Source: `IMPLEMENTATION_PLAN.md`
- Base ref: 8c6e0e3f6b87da8a192358e8fda2ea7dce58a448
- Head ref: 375790750fc858cba18ec70ba22c986da399e8b4

# Summary

HP-027A adds the `ApparentPlaceCalculator` request-routing boundary, routes
geometric, astrometric, apparent, and topocentric correction modes to the
expected frame targets, and bypasses apparent-place processing for geometric
engine requests. The review finding about astrometric requests with unsupported
extra correction flags was fixed by keeping geocentric non-Earth-orientation
requests on the astrometric/GCRS path while reporting unsupported refraction as
degraded metadata. Focused and configured tests pass, and the task is ready for
acceptance.

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

- Finding: Astrometric requests with extra unsupported flags route as apparent
  - Original severity: MAJOR
  - Closure status: Resolved
  - Notes: `requestModeForCorrections()` now routes diurnal parallax to topocentric, Earth-orientation requests to apparent, and other non-geometric geocentric combinations to astrometric/GCRS. Regression coverage verifies `Astrometric | AtmosphericRefraction` stays on GCRS and reports `CorrectionUnavailable`.

# Findings

No findings.

# Test assessment

Tests added or updated for this task include calculator-level routing coverage
for geometric, astrometric, astrometric plus unsupported refraction, apparent,
and refraction-only degraded metadata, plus engine-level coverage proving
geometric solar-system and star requests bypass apparent-place processing.

Tests run:

- `cmake --build build-ralph --target skygate-ephemeris-apparent-place-calculator-tests skygate-ephemeris-highprecision-engine-tests` - PASS
- `ctest --test-dir build-ralph --output-on-failure -R "skygate-ephemeris-(highprecision-engine|apparent-place-calculator)-tests"` - PASS
- `ctest --test-dir build-ralph --output-on-failure` - PASS, 59/59 tests

The coverage is appropriate for this routing-boundary task. Numerical apparent
correction algorithms remain intentionally deferred to later HP-027 subtasks.

# Regression risk

Low

The changed behavior is localized to the new apparent-place routing boundary
and high-precision engine dispatch. The review regression is covered, and the
full configured test suite passes.

# Out-of-scope observations

- The working tree contains an uncommitted include-order-only change in
  `libs/skygate-ephemeris/src/engine/simple/SimpleEphemerisEngine.cpp` that is
  not part of the HP-027A committed diff and was not committed by verification.
- `IMPLEMENTATION_PLAN.md`, `specs/high-precision-ephemeris-engine.md`, and
  prompt/support files under `.ralph/` are currently untracked in this working
  tree, but they were available for verification.

# Final recommendation

PASS: ready for final acceptance or merge.
