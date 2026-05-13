## Task
- ID: HP-016
- Title: Implement EOP interpolation and fallback behavior

## Status
READY

## Acceptance criteria claimed
- [x] Interpolates UT1-UTC and polar motion from Earth-orientation provider rows
- [x] Handles exact samples, interpolated samples, range boundaries, and outside-range fallback
- [x] Reports degraded metadata for stale, predicted, missing, and out-of-range EOP cases
- [x] Adds focused Qt Test coverage for EOP sampling behavior
- [x] Relevant ephemeris tests pass

## Files changed
- `libs/skygate-ephemeris/include/skygate/ephemeris/EarthOrientationProvider.hpp`
- `libs/skygate-ephemeris/src/engine/highprecision/EarthOrientationProvider.cpp`
- `libs/skygate-ephemeris/tests/highprecision/EarthOrientationProviderTests.cpp`

## Important notes
- `ctest --test-dir build-ralph -R '^skygate-ephemeris-' --output-on-failure` passed.
- Full `ctest --test-dir build-ralph --output-on-failure` reached 110/111 passing and failed only in `skygate-ui-qml-main-window-tests`, matching the existing unrelated HP-053 task.
