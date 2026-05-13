## Task
- ID: HP-012
- Title: Implement UTC, TAI, and TT conversion

## Status
READY

## Acceptance criteria claimed
- [x] UTC to TAI conversion implemented using `LeapSecondProvider`
- [x] UTC to TT conversion implemented with TT-TAI offset
- [x] TAI/TT conversion helpers exposed through `ITimeScaleService`
- [x] UTC leap-second civil labels such as `23:59:60` supported
- [x] Degraded/failure status returned for missing or out-of-range leap-second data
- [x] Tests added for normal UTC dates, leap-second boundaries, `23:59:60`, table range boundaries, and missing-table degraded fallback
- [x] Existing tests pass

## Files changed
- `libs/skygate-ephemeris/include/skygate/ephemeris/Types.hpp`
- `libs/skygate-ephemeris/include/skygate/ephemeris/TimeScaleService.hpp`
- `libs/skygate-ephemeris/src/engine/highprecision/TimeScaleService.cpp`
- `libs/skygate-ephemeris/CMakeLists.txt`
- `libs/skygate-ephemeris/tests/CMakeLists.txt`
- `libs/skygate-ephemeris/tests/highprecision/TimeScaleServiceTests.cpp`

## Important notes
- Verification run: `cmake --build build-ralph`
- Verification run: `ctest --test-dir build-ralph --output-on-failure`
