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
