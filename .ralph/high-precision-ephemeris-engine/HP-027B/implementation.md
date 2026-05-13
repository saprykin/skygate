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

## Review fixes
- Review verdict addressed: NEEDS_FIX
- Findings addressed:
  - Finding title: Missing reference coverage for light-time-corrected output
    - Action: Fixed
    - Notes: Added a Horizons-backed Mars light-time fixture with receive-time Earth barycentric state, three retarded Mars barycentric states, expected astrometric RA/Dec, and a dedicated test that validates call epochs and final light-time-corrected output.
- Files changed during fix pass:
  - `libs/skygate-ephemeris/tests/highprecision/SolarSystemStateCalculatorTests.cpp`
  - `libs/skygate-ephemeris/tests/fixtures/ephemeris/light_time_solar_system_mars.csv`
  - `.ralph/high-precision-ephemeris-engine/HP-027B/implementation.md`
  - `.ralph/high-precision-ephemeris-engine/HP-027B/fix.md`
- Tests run after fix:
  - `cmake --build build-ralph --target skygate-ephemeris-solar-system-state-calculator-tests`: PASS
  - `ctest --test-dir build-ralph -R skygate-ephemeris-solar-system-state-calculator-tests --output-on-failure`: PASS
  - `ctest --test-dir build-ralph --output-on-failure`: PASS
  - `git diff --check ca46d8df3fafb9aecf04e518820b1912432aea4f..HEAD && git diff --check`: PASS
- Remaining concerns: None.
