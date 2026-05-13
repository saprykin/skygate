## Task
- ID: HP-035
- Title: Add time and EOP validation test targets

## Status
READY

## Acceptance criteria claimed
- [x] Time/EOP validation tests are registered as CTest targets.
- [x] UTC/TAI/TT/TDB/UT1 conversion coverage remains registered.
- [x] Leap-second edge behavior coverage remains registered.
- [x] BCE/no-year-zero conversion coverage added.
- [x] EOP loading, interpolation, and fallback warning coverage remains registered.
- [x] Existing tests pass.

## Files changed
- `libs/skygate-ephemeris/tests/CMakeLists.txt`
- `libs/skygate-ephemeris/tests/highprecision/TimeScaleServiceTests.cpp`

## Important notes
- Added `validation;time-eop` CTest labels to the existing Delta T, Earth
  orientation, leap-second, and time-scale service validation targets.
- Verified with `ctest --test-dir build-ralph -L time-eop --output-on-failure`.
- Verified the full core test tree with `ctest --test-dir build-ralph --output-on-failure`.
