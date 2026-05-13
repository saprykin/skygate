# Verdict

PASS

# Task verified

- ID: HP-027G
- Title: Add apparent RA/Dec Horizons validation
- Source: `IMPLEMENTATION_PLAN.md`, `specs/high-precision-ephemeris-engine.md`
- Base ref: 7274992aea1258d4cb2867621b63aa5ea64bd9dc
- Head ref: 698ed464c843c45e488ac452171b2e0f53d36f64

# Summary

HP-027G added a Horizons-backed geocentric apparent Mars RA/Dec fixture, registered an apparent RA/Dec validation test, and changed the apparent-place precession/nutation route to use a true-equator/equinox-of-date frame instead of CIRS. The review identified one major frame-transform safety issue and one environment limitation. The fix restricts the new true-equator/equinox frame to explicitly supported GCRS-like transforms, adds regression coverage for rejected terrestrial transforms, and documents that the numeric Horizons assertion requires a high-precision-enabled ERFA/CALCEPH build. The available build and tests pass, and the task is ready for acceptance.

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

- Finding: True-equator/equinox frame is allowed through CIRS terrestrial stages
  - Original severity: MAJOR
  - Closure status: Resolved
  - Notes: `FrameTransformer.cpp` now handles only direct GCRS-like transforms for `TrueEquatorAndEquinox` and returns `CorrectionUnavailable` for unsupported terrestrial combinations before the composed CIRS/TIRS/ITRS path. `FrameTransformerTests.cpp` adds regression cases for true-equator/equinox to and from TIRS and ITRS. The regression test target is not generated in the current high-precision-disabled build, but the source fix matches the review recommendation.

- Finding: Numeric Horizons validation is skipped in the available build
  - Original severity: QUESTION
  - Closure status: Justified
  - Notes: The current `build-ralph` cache has `SKYGATE_ENABLE_HIGH_PRECISION_EPHEMERIS=OFF` and `calceph_DIR-NOTFOUND`, so the ERFA-backed numeric assertion is expected to skip locally. The validation test still loads and checks fixture metadata in this build, and the numeric assertion remains active for high-precision-enabled builds where ERFA/CALCEPH are available.

# Findings

No findings.

# Test assessment

The task adds `skygate-ephemeris-apparent-radec-validation-tests` and a fixture with required source, source URL, API parameters, generated date, source frame, time scale, target, observer, expected RA/Dec, and angular tolerance metadata. Fixture loading validates required metadata and numeric fields. Existing apparent-place routing tests were updated for the true-equator/equinox target frame, and the fix adds frame-transformer regression coverage for unsupported true-equator/equinox terrestrial transforms.

Tests run:
- `cmake --build build-ralph --target skygate-ephemeris-apparent-radec-validation-tests skygate-ephemeris-apparent-place-calculator-tests skygate-ephemeris-fixture-support-tests`: PASS
- `ctest --test-dir build-ralph --output-on-failure -R 'skygate-ephemeris-(apparent-radec-validation|apparent-place-calculator|frame-transformer|fixture-support)-tests'`: PASS, 3/3 available matching tests
- `./build-ralph/libs/skygate-ephemeris/tests/skygate-ephemeris-apparent-radec-validation-tests -v2`: PASS with one expected skip for unavailable ERFA-backed numeric validation
- `ctest --test-dir build-ralph --output-on-failure`: PASS, 61/61 tests

The frame-transformer regression test could not be executed in this `build-ralph` tree because high precision is disabled and `skygate-ephemeris-frame-transformer-tests` is not generated. The numeric Horizons tolerance assertion also could not be executed locally for the same dependency/configuration reason.

# Regression risk

Low

The implemented behavior is narrowly scoped to apparent RA/Dec validation and the new true-equator/equinox frame path. The prior unsafe composed-transform behavior was blocked before the shared CIRS/TIRS/ITRS pipeline, and the available full test suite passes.

# Out-of-scope observations

- The current local `build-ralph` configuration cannot prove the high-precision ERFA/CALCEPH numeric path. CI or a developer environment with `SKYGATE_ENABLE_HIGH_PRECISION_EPHEMERIS=ON` and CALCEPH installed should run the numeric validation lane.

# Final recommendation

PASS: ready for final acceptance or merge.
