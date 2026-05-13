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
