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

## Review fixes
- Review verdict addressed: NEEDS_FIX
- Findings addressed:
  - Finding title: Reverse conversions ignore leap-second table validity
    - Action: Fixed
    - Notes: `TAI -> UTC` now maps the leap-second table UTC validity range into TAI before accepting a reverse lookup, and `TT -> UTC` inherits the same check through the existing TT-to-TAI path. Out-of-range reverse conversions fail without fallback or return degraded status with range/fallback warnings when fallback is allowed.
  - Finding title: Public civil-to-epoch conversion loses leap-second labels
    - Action: Fixed
    - Notes: `astronomicalEpochFromCivilDateTime()` now rejects `second == 60` so ordinary civil-day epoch construction cannot normalize a leap-second label to next midnight. `convertCivilDateTime()` derives leap-second instants from `23:59:59 + 1s` and continues to validate them against the configured leap-second table.
- Files changed during fix pass:
  - `libs/skygate-ephemeris/include/skygate/ephemeris/Types.hpp`
  - `libs/skygate-ephemeris/src/engine/highprecision/TimeScaleService.cpp`
  - `libs/skygate-ephemeris/tests/engine/EphemerisApiModelTests.cpp`
  - `libs/skygate-ephemeris/tests/highprecision/TimeScaleServiceTests.cpp`
  - `.ralph/high-precision-ephemeris-engine/HP-012/implementation.md`
  - `.ralph/high-precision-ephemeris-engine/HP-012/fix.md`
- Tests run after fix:
  - `cmake --build build-ralph --target skygate-ephemeris-time-scale-service-tests skygate-ephemeris-api-model-tests` - PASS
  - `ctest --test-dir build-ralph -R 'skygate-ephemeris-(time-scale-service|api-model)-tests' --output-on-failure` - PASS
  - `ctest --test-dir build-ralph -L 'highprecision|unit' --output-on-failure` - PASS
  - `ctest --test-dir build-ralph --output-on-failure` - PASS
- Remaining concerns: None.
