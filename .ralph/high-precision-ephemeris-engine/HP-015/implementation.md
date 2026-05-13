## Task
- ID: HP-015
- Title: Add Earth-orientation data loading

## Status
READY

## Acceptance criteria claimed
- [x] Public Earth-orientation provider interface added for UT1-UTC and polar motion table data
- [x] Table-backed loader added for bundled or installed EOP text assets
- [x] Version, source, prediction interval, validity range, stale, missing, and malformed status metadata exposed
- [x] Loader tests added for valid data, malformed rows, missing values, prediction metadata, stale data, and missing data
- [x] Full build succeeds in `build-ralph`

## Files changed
- `libs/skygate-ephemeris/include/skygate/ephemeris/EarthOrientationProvider.hpp`
- `libs/skygate-ephemeris/include/skygate/ephemeris/EphemerisDataSnapshot.hpp`
- `libs/skygate-ephemeris/src/engine/highprecision/EarthOrientationProvider.cpp`
- `libs/skygate-ephemeris/CMakeLists.txt`
- `libs/skygate-ephemeris/tests/CMakeLists.txt`
- `libs/skygate-ephemeris/tests/highprecision/EarthOrientationProviderTests.cpp`
- `libs/skygate-ephemeris/tests/highprecision/HighPrecisionEphemerisEngineTests.cpp`

## Important notes
- `ctest --test-dir build-ralph --output-on-failure` ran with 110/111 tests passing.
- The only failure was `skygate-ui-qml-main-window-tests`, matching the existing HP-053 QML toolbar regression task.
- `skygate-ephemeris-earth-orientation-provider-tests` passed.
