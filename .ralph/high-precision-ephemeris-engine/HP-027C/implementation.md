## Task
- ID: HP-027C
- Title: Implement stellar aberration and gravitational light deflection

## Status
READY

## Acceptance criteria claimed
- [x] Correction-flag-controlled stellar aberration implemented
- [x] Correction-flag-controlled gravitational light deflection implemented
- [x] Solar-system state inputs from the high-precision kernel provider are used
- [x] Requested-but-unavailable correction inputs produce warnings
- [x] Tests added for enabled, disabled, and unavailable correction inputs
- [x] Existing tests pass

## Files changed
- `libs/skygate-ephemeris/src/engine/highprecision/HighPrecisionEphemerisEngine.hpp`
- `libs/skygate-ephemeris/src/engine/highprecision/CalcephKernelProvider.hpp`
- `libs/skygate-ephemeris/src/engine/highprecision/CalcephKernelProvider.cpp`
- `libs/skygate-ephemeris/src/engine/highprecision/SolarSystemStateCalculator.cpp`
- `libs/skygate-ephemeris/tests/highprecision/SolarSystemStateCalculatorTests.cpp`

## Important notes
- CALCEPH-backed kernel states now carry optional velocity in addition to position, so stellar aberration can use Earth barycentric velocity when available.
- Verification run: `ctest --test-dir build-ralph --output-on-failure` passed 59/59 tests.
