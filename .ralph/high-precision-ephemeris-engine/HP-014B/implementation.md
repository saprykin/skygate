## Task
- ID: HP-014B
- Title: Implement UT1 conversion policy

## Status
READY

## Acceptance criteria claimed
- [x] `TimeScaleService` converts UTC to UT1 using interpolated Earth-orientation samples.
- [x] `TimeScaleService` converts UT1 back to UTC and routes UT1 through UTC for TAI, TT, and TDB.
- [x] Delta T fallback policy is available for missing or out-of-range Earth-orientation data when explicitly enabled.
- [x] Degraded conversion warnings are reported for stale EOP, predicted EOP, out-of-range EOP, missing EOP, Delta T fallback, and unavailable Delta T.
- [x] Tests cover exact EOP samples, interpolated samples, reverse UT1 conversion, stale/predicted EOP warnings, Delta T fallback, and disallowed fallback failure.
- [x] Relevant tests pass.

## Files changed
- `libs/skygate-ephemeris/include/skygate/ephemeris/TimeScaleService.hpp`
- `libs/skygate-ephemeris/src/engine/highprecision/TimeScaleService.cpp`
- `libs/skygate-ephemeris/tests/highprecision/TimeScaleServiceTests.cpp`

## Important notes
- `ctest --test-dir build-ralph -R skygate-ephemeris-time-scale-service-tests --output-on-failure` passes.
- `cmake --build build-ralph -j2` passes.
- Full `ctest --test-dir build-ralph --output-on-failure` passes 110/111 tests. The remaining failure is the pre-existing `skygate-ui-qml-main-window-tests` footer popup toolbar toggle failure already tracked by HP-053.
