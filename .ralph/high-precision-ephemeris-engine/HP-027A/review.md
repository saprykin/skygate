## Verdict

NEEDS_FIX

## Task reviewed

- ID: HP-027A
- Title: Add `ApparentPlaceCalculator` boundary and request mode routing
- Source: `IMPLEMENTATION_PLAN.md`
- Base ref: 8c6e0e3f6b87da8a192358e8fda2ea7dce58a448
- Head ref: 881245db724d0b7772943a22d4f09494256b6410

## Summary

The implementation adds `ApparentPlaceCalculator` request-mode routing, bypasses
apparent-place processing for geometric engine requests, and adds focused tests.
The main path is in place, and the configured test suite passes, but the
astrometric routing logic is too narrow for composed correction flags and can
route an astrometric request with an unsupported correction into the apparent
CIRS path.

## Checks performed

- [x] Read active task
- [x] Read implementation handoff
- [x] Read relevant specs
- [x] Inspected git diff
- [x] Inspected relevant tests
- [x] Ran relevant tests, if safe/applicable

## Findings

### Finding 1: Astrometric requests with extra unsupported flags route as apparent

Severity: MAJOR
File: `libs/skygate-ephemeris/src/engine/highprecision/ApparentPlaceCalculator.cpp`
Lines/functions: `requestModeForCorrections()`, lines 43-60

Problem:
`requestModeForCorrections()` only treats the exact
`EphemerisCorrectionFlags::Astrometric` composite as astrometric. If a caller
requests astrometric RA/Dec plus an unsupported correction such as
`AtmosphericRefraction`, the bit pattern is no longer exactly `Astrometric`.
Because the astrometric preset contains `StellarAberration`,
`GravitationalLightDeflection`, and `PrecessionNutation`, the current logic
falls through to `Apparent` and routes through `CelestialReferenceFrame::Cirs`
before reporting refraction as unavailable.

Why it matters:
HP-027A is specifically the boundary and routing task, not the numerical
correction implementation. Adding an unsupported correction flag should produce
structured degraded metadata without changing an otherwise astrometric RA/Dec
request into an apparent-frame transform. This also leaves the claimed
unsupported-mode coverage incomplete because the tests only cover
refraction-only routing, not refraction combined with an explicit request mode.

Recommended fix:
Classify the request mode independently from unsupported correction bits. At
minimum, preserve astrometric routing for `Astrometric` plus currently
unsupported corrections such as atmospheric refraction, report the unsupported
correction in metadata, and add a regression test for that combination. Also
add a direct geometric routing test for `ApparentPlaceCalculator` or clearly
document that geometric mode is intentionally handled only by the engine bypass.

## Test assessment

New tests cover astrometric-to-GCRS, apparent-to-CIRS, refraction-only degraded
metadata, and geometric engine bypasses for solar-system and star bodies. I ran:

- `cmake --build build-ralph --target skygate-ephemeris-apparent-place-calculator-tests`
- `cmake --build build-ralph --target skygate-ephemeris-highprecision-engine-tests`
- `ctest --test-dir build-ralph --output-on-failure -R "skygate-ephemeris-(highprecision-engine|apparent-place-calculator)-tests"`
- `ctest --test-dir build-ralph --output-on-failure`

All 59 configured tests passed. Missing coverage: astrometric routing with
additional unsupported correction flags, and a direct calculator-level
geometric-mode assertion if the calculator is expected to own that mode.

## Regression risk

Medium

The affected code is a new routing boundary, but it decides which frame
transform path runs for public correction-flag combinations.

## Out-of-scope observations

None.

## Final recommendation

NEEDS_FIX: fix listed findings, then rerun review.
