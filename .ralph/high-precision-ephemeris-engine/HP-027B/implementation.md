## Task
- ID: HP-027B
- Title: Implement solar-system light-time correction

## Status
READY

## Acceptance criteria claimed
- [x] Light-time correction is controlled by `EphemerisCorrectionFlags::LightTime`
- [x] Solar-system astrometric direction uses Earth at receive time and iterated target transmit time
- [x] Unavailable light-time inputs produce warnings and preserve geometric fallback results
- [x] Tests added for enabled light-time and unavailable-input fallback behavior
- [x] Existing tests pass

## Files changed
- `libs/skygate-ephemeris/src/engine/highprecision/SolarSystemStateCalculator.cpp`
- `libs/skygate-ephemeris/tests/highprecision/SolarSystemStateCalculatorTests.cpp`

## Important notes
- Verification run: `cmake --build build-ralph`
- Verification run: `ctest --test-dir build-ralph --output-on-failure`
