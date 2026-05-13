## Verdict

NEEDS_FIX

## Task reviewed

- ID: HP-027G
- Title: Add apparent RA/Dec Horizons validation
- Source: `IMPLEMENTATION_PLAN.md`, `specs/high-precision-ephemeris-engine.md`
- Base ref: 7274992aea1258d4cb2867621b63aa5ea64bd9dc
- Head ref: 4ab4b5243890537d43ff51ea3e51d2acaaf3abbe

## Summary

The implementation adds a Horizons-backed Mars apparent RA/Dec fixture, a new validation test, and a true-equator/equinox-of-date apparent frame path for geocentric apparent RA/Dec. The direct apparent RA/Dec path is directionally correct for the fixture semantics, but adding `TrueEquatorAndEquinox` as a peer of `Cirs` also makes unsupported terrestrial transforms return valid-looking results with incorrect frame semantics.

## Checks performed

- [x] Read active task
- [x] Read implementation handoff
- [x] Read relevant specs
- [x] Inspected git diff
- [x] Inspected relevant tests
- [x] Ran relevant tests, if safe/applicable

## Findings

### Finding 1: True-equator/equinox frame is allowed through CIRS terrestrial stages

Severity: MAJOR
File: `libs/skygate-ephemeris/src/engine/highprecision/FrameTransformer.cpp`
Lines/functions: 35-37, 517-535, `ErfaFrameTransformer::transformCelestialVector`

Problem:
`TrueEquatorAndEquinox` is assigned the same rank as `Cirs`, and unsupported composed transforms are not rejected once the target rank differs. A request such as `TrueEquatorAndEquinox -> Tirs` or `TrueEquatorAndEquinox -> Itrs` therefore enters the generic composed path and applies the existing rank-1 Earth-rotation stage as though the source were CIRS.

Why it matters:
True equator/equinox of date and CIRS share the date equator but not the right-ascension origin. Treating them interchangeably for Earth-rotation/terrestrial transforms can return a vector with `Valid` status and `EarthOrientation` metadata even though the orientation is wrong. This does not appear to affect the new HP-027G geocentric apparent RA/Dec path, which only needs `Gcrs -> TrueEquatorAndEquinox`, but it makes the new frame enum unsafe for other frame-transform requests.

Recommended fix:
Restrict `TrueEquatorAndEquinox` support to the explicitly implemented direct transforms to and from GCRS-like frames until the equinox/CIO origin conversion is implemented. For other combinations involving `TrueEquatorAndEquinox`, return `CorrectionUnavailable`/failed metadata instead of falling through the composed CIRS/TIRS/ITRS path. Add regression coverage for at least `TrueEquatorAndEquinox -> Tirs` or `TrueEquatorAndEquinox -> Itrs`.

### Finding 2: Numeric Horizons validation is skipped in the available build

Severity: QUESTION
File: `libs/skygate-ephemeris/tests/highprecision/ApparentRaDecValidationTests.cpp`
Lines/functions: 237-245, `computesGeocentricApparentRaDecAgainstHorizonsFixture`

Problem:
The validation test probes ERFA availability and calls `QSKIP` when high-precision support is not compiled in. In the current `build-ralph` configuration, `SKYGATE_ENABLE_HIGH_PRECISION_EPHEMERIS=OFF`, so the test target passes without executing the angular tolerance assertion at lines 258-264.

Why it matters:
HP-027G's verification asks for apparent RA/Dec validation tests to pass within documented tolerances. The current environment confirms fixture loading and test registration, but it does not prove the numeric Horizons comparison.

Recommended fix:
Ensure the HP-027G validation is exercised in a high-precision-enabled build or CI lane. If this repository intentionally permits simple-only builds to skip the numeric assertion, record that limitation in the verification notes and keep the validation target tied to a high-precision configuration where ERFA/CALCEPH are available.

## Test assessment

The new validation fixture has the required source, parameter, frame, time-scale, target, observer, expected-value, and tolerance metadata, and `loadRaDecFixture` enforces non-empty required metadata fields. The new validation test is registered and loads the apparent fixture. In this environment, the numeric comparison is skipped because `build-ralph` is configured with high precision disabled.

Tests run:
- `ctest --test-dir build-ralph --output-on-failure -R 'skygate-ephemeris-(apparent-radec-validation|apparent-place-calculator|frame-transformer|ephemeris-fixture-support)-tests'` passed the two matching available tests.
- `ctest --test-dir build-ralph --output-on-failure -R 'skygate-ephemeris-fixture-support-tests'` passed.
- `ctest --test-dir build-ralph --output-on-failure` passed 61/61 tests.
- `./build-ralph/libs/skygate-ephemeris/tests/skygate-ephemeris-apparent-radec-validation-tests -v2` showed the numeric validation test was skipped.
- `cmake --build build-ralph --target skygate-ephemeris-apparent-radec-validation-tests skygate-ephemeris-apparent-place-calculator-tests skygate-ephemeris-frame-transformer-tests skygate-ephemeris-fixture-support-tests` built the first two targets, then failed because `skygate-ephemeris-frame-transformer-tests` is not generated in the current high-precision-disabled build.

## Regression risk

Medium

The new apparent RA/Dec path is narrow, but the added frame enum changes the accepted state space of the shared frame transformer. Unsupported `TrueEquatorAndEquinox` composed transforms can now appear successful and produce incorrect terrestrial-frame vectors.

## Out-of-scope observations

The current `build-ralph` tree is configured with `SKYGATE_ENABLE_HIGH_PRECISION_EPHEMERIS=OFF`, so ERFA-specific frame transformer tests are not generated in this build. The implementation handoff says enabling high precision could not be verified here because `calceph` is not installed.

## Final recommendation

NEEDS_FIX: fix listed findings, then rerun review.
