## Task
- ID: HP-027A
- Title: Add `ApparentPlaceCalculator` boundary and request mode routing

## Status
READY

## Acceptance criteria claimed
- [x] `ApparentPlaceCalculator` routes geometric, astrometric, and apparent request modes
- [x] Geometric high-precision requests bypass apparent-place processing for solar-system and star bodies
- [x] Unsupported refraction-only correction routing reports structured degraded metadata
- [x] Tests added for apparent-place request mode dispatch
- [x] Existing tests pass

## Files changed
- `libs/skygate-ephemeris/src/engine/highprecision/ApparentPlaceCalculator.cpp`
- `libs/skygate-ephemeris/src/engine/highprecision/HighPrecisionEphemerisEngine.cpp`
- `libs/skygate-ephemeris/tests/CMakeLists.txt`
- `libs/skygate-ephemeris/tests/highprecision/ApparentPlaceCalculatorTests.cpp`
- `libs/skygate-ephemeris/tests/highprecision/HighPrecisionEphemerisEngineTests.cpp`

## Important notes
- Verification run: `cmake --build build-ralph`
- Verification run: `ctest --test-dir build-ralph --output-on-failure`

## Review fixes
- Review verdict addressed: NEEDS_FIX
- Findings addressed:
  - Finding title: Astrometric requests with extra unsupported flags route as apparent
  - Action: Fixed
  - Notes: `ApparentPlaceCalculator` now keeps geocentric requests without `EarthOrientation` in the astrometric/GCRS path even when unsupported extras such as `AtmosphericRefraction` are present. Refraction is still reported as unavailable degraded metadata.
- Files changed during fix pass:
  - `libs/skygate-ephemeris/src/engine/highprecision/ApparentPlaceCalculator.cpp`
  - `libs/skygate-ephemeris/tests/highprecision/ApparentPlaceCalculatorTests.cpp`
  - `.ralph/high-precision-ephemeris-engine/HP-027A/implementation.md`
  - `.ralph/high-precision-ephemeris-engine/HP-027A/fix.md`
- Tests run after fix:
  - `cmake --build build-ralph --target skygate-ephemeris-apparent-place-calculator-tests skygate-ephemeris-highprecision-engine-tests` - PASS
  - `ctest --test-dir build-ralph --output-on-failure -R "skygate-ephemeris-(highprecision-engine|apparent-place-calculator)-tests"` - PASS
  - `ctest --test-dir build-ralph --output-on-failure` - PASS
- Remaining concerns: None.
