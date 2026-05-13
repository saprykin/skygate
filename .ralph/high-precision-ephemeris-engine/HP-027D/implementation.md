## Task
- ID: HP-027D
- Title: Integrate precession and nutation into apparent RA/Dec

## Status
READY

## Acceptance criteria claimed
- [x] Precession/nutation correction flag routes apparent-place processing through the frame transformer to CIRS.
- [x] Requests without precession/nutation keep GCRS output and do not apply the correction implicitly.
- [x] Apparent-place results use the transformed vector output for RA/Dec.
- [x] Degraded frame/time/EOP metadata from the frame transformer propagates into apparent-place result metadata.
- [x] Touched C++ files were formatted with `clang-format`.
- [x] Existing tests pass.

## Files changed
- `libs/skygate-ephemeris/src/engine/highprecision/ApparentPlaceCalculator.cpp`
- `libs/skygate-ephemeris/tests/highprecision/ApparentPlaceCalculatorTests.cpp`
- `.ralph/high-precision-ephemeris-engine/HP-027D/implementation.md`

## Important notes
- Verification run: `ctest --test-dir build-ralph --output-on-failure` passed 59/59 tests.

## Review fixes
- Review verdict addressed: NEEDS_FIX
- Findings addressed:
  - Finding title: Required degraded-input propagation test is missing
  - Action: Fixed
  - Notes: Added apparent-place coverage that constructs a real `ErfaFrameTransformer` with a degraded TT conversion service, requests `PrecessionNutation`, and verifies degraded transformer metadata reaches apparent-place result metadata. The test skips only when the current build lacks ERFA-backed transforms.
- Files changed during fix pass:
  - `libs/skygate-ephemeris/tests/highprecision/ApparentPlaceCalculatorTests.cpp`
  - `.ralph/high-precision-ephemeris-engine/HP-027D/implementation.md`
  - `.ralph/high-precision-ephemeris-engine/HP-027D/fix.md`
- Tests run after fix:
  - `cmake --build build-ralph --target skygate-ephemeris-apparent-place-calculator-tests` - PASS
  - `ctest --test-dir build-ralph -R '^skygate-ephemeris-apparent-place-calculator-tests$' --output-on-failure` - PASS
  - `ctest --test-dir build-ralph --output-on-failure` - PASS, 59/59 tests
- Remaining concerns:
  - `build-ralph` is configured with `SKYGATE_ENABLE_HIGH_PRECISION_EPHEMERIS=OFF`, so the real ERFA branch in the new test is skipped in this local build and will execute in high-precision-enabled builds.
