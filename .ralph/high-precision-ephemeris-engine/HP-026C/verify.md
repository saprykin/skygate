# Verdict

PASS

# Task verified

- ID: HP-026C
- Title: Add `FrameTransformer` orchestration and per-stage metadata
- Source: `IMPLEMENTATION_PLAN.md`
- Base ref: 4ff76a5a8ae762033fd8ca0eacae35f496f9dee1
- Head ref: ccc26b8b81ec62430b89c78eae8ea3b3fcf54229

# Summary

HP-026C added request-scoped `FrameTransformer` orchestration, per-stage transform metadata, aggregate metadata propagation, and focused tests for composed stages, cached conversions, unavailable stages, EOP reuse, and ICRS boundary reporting. The review found two MAJOR issues: duplicated EOP lookup work across composed terrestrial stages and ICRS requests being reported as GCRS stages. The fix pass addressed both. The configured simple-only build and registered tests pass, while the high-precision-only frame transformer test target remains unavailable in this environment because `calceph` is not installed. Final judgment: ready for acceptance.

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

- Finding: Composed terrestrial transforms still duplicate EOP lookup work
  - Original severity: MAJOR
  - Closure status: Resolved
  - Notes: `FrameTransformContext::ut1Epoch()` now derives UT1 from the request-scoped Earth-orientation sample when an EOP provider is available, and the TIRS/ITRS path reuses `FrameTransformContext::earthOrientation()`. The added recording-provider test uses production `LeapSecondTimeScaleService` wiring and verifies the composed GCRS-to-ITRS path performs only one frame-transform EOP sample.

- Finding: ICRS requests are reported as GCRS stages
  - Original severity: MAJOR
  - Closure status: Resolved
  - Notes: Stage metadata now preserves the actual request source frame for the first forward stage and the actual request target frame for the final reverse stage. Added tests cover `Icrs -> Itrs` and `Itrs -> Icrs` metadata boundaries.

# Findings

No findings.

# Test assessment

Source tests were added in `libs/skygate-ephemeris/tests/highprecision/FrameTransformerTests.cpp` for composed per-stage metadata, request-scoped time-scale conversion reuse, request-scoped EOP reuse, ICRS boundary metadata, and unavailable-stage metadata. These tests directly cover HP-026C and the two review fixes.

Commands run:

- `git diff --check 4ff76a5a8ae762033fd8ca0eacae35f496f9dee1..HEAD`: PASS
- `cmake -S . -B build-ralph -DSKYGATE_BUILD_UI=OFF -DSKYGATE_BUILD_TESTS=ON -DSKYGATE_ENABLE_HIGH_PRECISION_EPHEMERIS=OFF`: PASS
- `cmake --build build-ralph --target skygate-ephemeris --parallel`: PASS
- `ctest --test-dir build-ralph --output-on-failure`: PASS, 58/58 tests
- `cmake -S . -B build-ralph -DSKYGATE_BUILD_UI=OFF -DSKYGATE_BUILD_TESTS=ON -DSKYGATE_ENABLE_HIGH_PRECISION_EPHEMERIS=ON`: FAIL, configure is blocked because `calceph` is not installed.

The task-specific `skygate-ephemeris-frame-transformer-tests` target could not be generated or run in this environment due to the missing `calceph` dependency. The source coverage is present and should be run in a high-precision-enabled environment with CALCEPH installed.

# Regression risk

Low

The changed surface is limited to high-precision frame-transform orchestration and its focused tests. The fix removes duplicated EOP sampling and corrects metadata labeling without changing the established SOFA/ERFA matrix helpers.

# Out-of-scope observations

- The high-precision CMake configuration still depends on an external `calceph` installation, so CI or developer environments need that dependency before the frame-transformer test target can execute.

# Final recommendation

PASS: ready for final acceptance or merge.
