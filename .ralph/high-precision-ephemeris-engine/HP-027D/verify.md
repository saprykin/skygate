# Verdict

PASS

# Task verified

- ID: HP-027D
- Title: Integrate precession and nutation into apparent RA/Dec
- Source: IMPLEMENTATION_PLAN.md / specs/high-precision-ephemeris-engine.md
- Base ref: 8d4b5c0
- Head ref: 8ab7288

# Summary

HP-027D changed apparent-place request routing so the precession/nutation flag sends GCRS vectors through the frame transformer to CIRS, while requests without that flag remain in GCRS. The implementation also uses the transformed vector for output RA/Dec and merges transformer metadata back into apparent-place metadata. Review found one missing degraded-input integration test; the fix added a real `ErfaFrameTransformer` coverage path with a degraded TT conversion service, plus handoff notes. The current `build-ralph` tree is high-precision-disabled, so that ERFA-backed test is present but skipped locally; the focused and full available suites pass. The task is ready for acceptance.

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

- Finding: Required degraded-input propagation test is missing
  - Original severity: MAJOR
  - Closure status: Resolved
  - Notes: The fix added `propagatesDegradedRealFrameTransformMetadataForPrecessionNutation()`, which constructs an `ErfaFrameTransformer` with a degraded TT conversion service and verifies the resulting apparent-place metadata carries degraded status, time-scale warnings, and the applied precession/nutation flag. In the current `build-ralph` configuration this ERFA-backed branch skips because `SKYGATE_ENABLE_HIGH_PRECISION_EPHEMERIS=OFF`, but the test is in place for high-precision-enabled builds.

# Findings

No findings.

# Test assessment

Relevant tests exist in `libs/skygate-ephemeris/tests/highprecision/ApparentPlaceCalculatorTests.cpp`. They cover precession/nutation-enabled CIRS routing, requests without precession/nutation staying in GCRS, transformed-vector RA/Dec output, mocked degraded transform metadata propagation, and the newly added real-frame-transform degraded TT conversion path.

Commands run:

- `cmake --build build-ralph --target skygate-ephemeris-apparent-place-calculator-tests` - PASS
- `ctest --test-dir build-ralph -R '^skygate-ephemeris-apparent-place-calculator-tests$' --output-on-failure` - PASS
- `ctest --test-dir build-ralph --output-on-failure` - PASS, 59/59 tests
- `./build-ralph/libs/skygate-ephemeris/tests/skygate-ephemeris-apparent-place-calculator-tests -v2` - PASS, 9 passed and 1 skipped

The skipped test is the ERFA-backed degraded-transform integration test, skipped because the current build cache has `SKYGATE_ENABLE_HIGH_PRECISION_EPHEMERIS:BOOL=OFF`.

# Regression risk

Low

The production change is a narrow correction-flag routing fix in `ApparentPlaceCalculator`, and the available focused and full test suites pass. The remaining limitation is only that the real ERFA degraded-input branch was not executable in this local high-precision-disabled build.

# Out-of-scope observations

- `ApparentPlaceCalculator::mergeMetadata` still promotes only `Failed` and `Degraded` transformer statuses. If future frame transforms return `OutOfRange` or `Unsupported` with a vector, those statuses would not currently propagate. This was noted in review as out of scope and was not introduced by HP-027D.

# Final recommendation

PASS: ready for final acceptance or merge.
