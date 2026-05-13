## Task
- ID: HP-027G
- Title: Add apparent RA/Dec Horizons validation

## Status
READY

## Acceptance criteria claimed
- [x] Geocentric apparent RA/Dec validation fixture added with Horizons source parameters, frame, time scale, target, observer, expected values, and tolerance.
- [x] Apparent RA/Dec validation test target registered.
- [x] Apparent-place precession/nutation output now uses a true-equator/equinox-of-date frame for Horizons apparent RA/Dec semantics.
- [x] Tests pass in the available `build-ralph` configuration.

## Files changed
- `libs/skygate-ephemeris/src/engine/highprecision/ApparentPlaceCalculator.cpp`
- `libs/skygate-ephemeris/src/engine/highprecision/ErfaAstrometry.hpp`
- `libs/skygate-ephemeris/src/engine/highprecision/ErfaAstrometry.cpp`
- `libs/skygate-ephemeris/src/engine/highprecision/FrameTransformer.hpp`
- `libs/skygate-ephemeris/src/engine/highprecision/FrameTransformer.cpp`
- `libs/skygate-ephemeris/tests/CMakeLists.txt`
- `libs/skygate-ephemeris/tests/fixtures/ephemeris/apparent_solar_system_mars_smoke.json`
- `libs/skygate-ephemeris/tests/highprecision/ApparentPlaceCalculatorTests.cpp`
- `libs/skygate-ephemeris/tests/highprecision/ApparentRaDecValidationTests.cpp`

## Important notes
- `ctest --test-dir build-ralph --output-on-failure` passed: 61/61 tests.
- `cmake -S . -B build-ralph -DSKYGATE_ENABLE_HIGH_PRECISION_EPHEMERIS=ON` could not be verified in this environment because `calceph` is not installed.
- The new validation test skips the ERFA-backed numeric assertion in simple-only builds where high-precision dependencies are disabled.

## Review fixes
- Review verdict addressed: NEEDS_FIX
- Findings addressed:
  - Finding title: True-equator/equinox frame is allowed through CIRS terrestrial stages
    - Action: Fixed
    - Notes: `TrueEquatorAndEquinox` now returns `CorrectionUnavailable` for unsupported non-GCRS-like transforms instead of entering the composed CIRS/TIRS/ITRS path. Regression coverage rejects both TIRS and ITRS directions.
  - Finding title: Numeric Horizons validation is skipped in the available build
    - Action: Not applicable
    - Notes: The current `build-ralph` cache has `SKYGATE_ENABLE_HIGH_PRECISION_EPHEMERIS=OFF` and `calceph_DIR-NOTFOUND`, so the local build can only verify fixture loading and test registration. The numeric assertion remains tied to high-precision builds where ERFA/CALCEPH are available.
- Files changed during fix pass:
  - `libs/skygate-ephemeris/src/engine/highprecision/FrameTransformer.cpp`
  - `libs/skygate-ephemeris/tests/highprecision/FrameTransformerTests.cpp`
  - `.ralph/high-precision-ephemeris-engine/HP-027G/implementation.md`
  - `.ralph/high-precision-ephemeris-engine/HP-027G/fix.md`
- Tests run after fix:
  - `cmake --build build-ralph --target skygate-ephemeris-apparent-radec-validation-tests skygate-ephemeris-apparent-place-calculator-tests`: PASS
  - `cmake --build build-ralph --target skygate-ephemeris-frame-transformer-tests`: NOT RUN; target is not generated in the current high-precision-disabled `build-ralph` tree.
  - `ctest --test-dir build-ralph --output-on-failure -R 'skygate-ephemeris-(apparent-radec-validation|apparent-place-calculator|frame-transformer|fixture-support)-tests'`: PASS
  - `ctest --test-dir build-ralph --output-on-failure -R 'skygate-ephemeris-fixture-support-tests'`: PASS
  - `./build-ralph/libs/skygate-ephemeris/tests/skygate-ephemeris-apparent-radec-validation-tests -v2`: PASS with one expected skip because ERFA-backed apparent RA/Dec validation is unavailable in this simple-only build.
  - `ctest --test-dir build-ralph --output-on-failure`: PASS
- Remaining concerns: The new frame-transformer regression test could not be executed locally because `build-ralph` is configured with high precision disabled and does not generate `skygate-ephemeris-frame-transformer-tests`.
